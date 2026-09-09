#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <opencv2/core/core.hpp>

#if defined(__GNUC__) || defined(__clang__)
// This header is compiled independently into many plugin DSOs. Its C++
// implementation must never become part of the plugin's public dynamic ABI;
// otherwise ELF symbol interposition can bind one plugin to another's methods.
#pragma GCC visibility push(hidden)
#endif

namespace jdk_tracking {

constexpr int kFallbackTrackBase = 100000;

inline bool is_fallback_track_id(int track_id) {
	return track_id >= kFallbackTrackBase;
}

inline std::string format_track_label(int track_id) {
	if (!is_fallback_track_id(track_id)) {
		return "ID:" + std::to_string(track_id);
	}
	return "ID:F" + std::to_string(std::max(1, track_id - kFallbackTrackBase + 1));
}

inline float rect_iou(const cv::Rect& a, const cv::Rect& b) {
	const cv::Rect inter = a & b;
	const float inter_area = static_cast<float>(inter.area());
	const float union_area = static_cast<float>(a.area() + b.area()) - inter_area;
	return union_area > 0.0f ? inter_area / union_area : 0.0f;
}

inline float rect_center_distance_ratio(const cv::Rect& a, const cv::Rect& b) {
	const float ax = a.x + a.width * 0.5f;
	const float ay = a.y + a.height * 0.5f;
	const float bx = b.x + b.width * 0.5f;
	const float by = b.y + b.height * 0.5f;
	const float dx = ax - bx;
	const float dy = ay - by;
	const float diag = std::sqrt(static_cast<float>(std::max(1, a.width * a.width + a.height * a.height)));
	return std::sqrt(dx * dx + dy * dy) / std::max(1.0f, diag);
}

inline float rect_size_ratio(const cv::Rect& a, const cv::Rect& b) {
	const float area_a = static_cast<float>(std::max(1, a.area()));
	const float area_b = static_cast<float>(std::max(1, b.area()));
	return std::min(area_a, area_b) / std::max(area_a, area_b);
}

inline float fallback_match_score(const cv::Rect& current, const cv::Rect& previous) {
	const float iou = rect_iou(current, previous);
	if (iou >= 0.30f) return 1.0f + iou;

	const float center_dist = rect_center_distance_ratio(current, previous);
	const float size_ratio = rect_size_ratio(current, previous);
	if (iou >= 0.08f && center_dist <= 0.75f && size_ratio >= 0.35f) {
		return iou + (1.0f - center_dist) * 0.25f + size_ratio * 0.15f;
	}
	return 0.0f;
}

// Converts short-lived tracker IDs into a stable business-lifecycle ID.
//
// Important distinction from fallback assignment:
// - fallback only runs when a tracker did not return an ID;
// - this resolver also handles a *new positive tracker ID* that is spatially
//   continuous with a recently observed target.
//
// Established raw->stable aliases are always assigned first. New aliases then
// use conservative class-aware, one-to-one geometric matching against a
// short-lived motion prediction. Long disappearance must be handled by
// business-specific ReID/identity evidence instead of extending this window.
class StableTrackIdResolver {
public:
    struct Statistics {
        std::uint64_t frames{0};
        std::uint64_t detections{0};
        std::uint64_t raw_id_missing{0};
        std::uint64_t duplicate_raw_id{0};
        std::uint64_t alias_hits{0};
        std::uint64_t alias_rejected{0};
        std::uint64_t reconnected{0};
        std::uint64_t reconnect_gap_1{0};
        std::uint64_t reconnect_gap_2_to_6{0};
        std::uint64_t reconnect_gap_over_6{0};
        std::uint64_t ambiguous_rejected{0};
        std::uint64_t allocated{0};
        std::uint64_t allocated_no_candidate{0};
        std::uint64_t allocated_ambiguous{0};
        std::uint64_t allocated_conflict{0};
        std::uint64_t expired{0};
        std::uint64_t expired_single_observation{0};
        std::uint64_t expired_short_lifecycle{0};
        std::size_t live_states{0};
        std::size_t live_aliases{0};
    };

    std::vector<int> resolve(const std::vector<int>& raw_ids,
                             const std::vector<cv::Rect>& rects,
                             const std::vector<int>& class_ids,
                             int frame_index,
                             int max_missing_frames) {
        // aliases_, states_, and next_stable_id_ form one state invariant.
        // Serialize access so parallel callbacks and task lifecycle events
        // cannot invalidate iterators or corrupt the unordered_map storage.
        std::lock_guard<std::mutex> lock(mutex_);
        const size_t count = std::min(raw_ids.size(), rects.size());
        std::vector<int> stable_ids(count, -1);
        std::unordered_set<int> claimed;
        std::unordered_map<int, int> raw_id_counts;
        for (size_t i = 0; i < count; ++i) {
            if (raw_ids[i] >= 0) ++raw_id_counts[raw_ids[i]];
        }
        const int ttl = std::max(0, max_missing_frames);

        ++statistics_.frames;
        statistics_.detections += count;
        for (size_t i = 0; i < count; ++i) {
            if (raw_ids[i] < 0) ++statistics_.raw_id_missing;
            else if (raw_id_counts[raw_ids[i]] != 1) ++statistics_.duplicate_raw_id;
        }

        cleanup(frame_index, ttl);

        // Pass 1: preserve established aliases before doing any spatial match.
        // Multiple historic raw IDs may point at the same stable lifecycle after
        // a tracker fracture/recovery. Select the geometrically best live owner
        // instead of letting detector iteration order decide who gets the ID.
        struct AliasCandidate {
            float score{0.0f};
            size_t detection_index{0};
            int raw_id{-1};
            int stable_id{-1};
        };
        std::vector<AliasCandidate> alias_candidates;
        std::vector<int> rejected_aliases;
        for (size_t i = 0; i < count; ++i) {
			// A duplicated raw ID is malformed tracker output. Treat all of its
			// detections as untrusted geometry fallback and never mutate aliases.
            if (raw_ids[i] < 0 || raw_id_counts[raw_ids[i]] != 1) continue;
            const auto alias = aliases_.find(raw_ids[i]);
            if (alias == aliases_.end()) continue;
            const auto state = states_.find(alias->second);
            if (state == states_.end() || claimed.contains(alias->second) ||
                frame_index - state->second.last_seen_frame > ttl) {
                continue;
            }
            const int gap = std::max(1, frame_index - state->second.last_seen_frame);
            const cv::Rect predicted = predict(state->second, gap);
            if (!alias_motion_is_plausible(rects[i], predicted, state->second, gap)) {
                // Trackers can recycle or accidentally resurrect a raw ID. A
                // teleport must start/reconnect by geometry rather than silently
                // binding a different person to the previous business identity.
                rejected_aliases.push_back(raw_ids[i]);
                ++statistics_.alias_rejected;
                continue;
            }
            alias_candidates.push_back(AliasCandidate{
                alias_match_score(rects[i], predicted, state->second),
                i, raw_ids[i], alias->second});
        }
        for (const int raw_id : rejected_aliases) aliases_.erase(raw_id);
        std::sort(alias_candidates.begin(), alias_candidates.end(),
            [](const AliasCandidate& lhs, const AliasCandidate& rhs) {
                if (lhs.score != rhs.score) return lhs.score > rhs.score;
                if (lhs.stable_id != rhs.stable_id) return lhs.stable_id < rhs.stable_id;
                return lhs.detection_index < rhs.detection_index;
            });
        std::unordered_set<size_t> alias_assigned_detections;
        for (const auto& candidate : alias_candidates) {
            if (alias_assigned_detections.contains(candidate.detection_index) ||
                claimed.contains(candidate.stable_id)) continue;
            stable_ids[candidate.detection_index] = candidate.stable_id;
            alias_assigned_detections.insert(candidate.detection_index);
            claimed.insert(candidate.stable_id);
            ++statistics_.alias_hits;
        }

        // Pass 2: build all viable reconnect candidates first, then assign the
        // strongest edges globally. Per-detection greedy matching makes the
        // result depend on detector output order and causes ID switches when
        // nearby targets cross.
        struct Candidate {
            float score{0.0f};
            size_t detection_index{0};
            int stable_id{-1};
        };
        std::vector<Candidate> candidates;
        for (size_t i = 0; i < count; ++i) {
			// raw_id < 0 means the tracker missed this detection. It still
			// participates in conservative geometry reconnect, providing one
			// shared fallback path for every plugin.
            if (stable_ids[i] >= 0) continue;
            const int class_id = class_at(class_ids, i);
            for (const auto& [stable_id, state] : states_) {
                if (claimed.contains(stable_id) ||
                    frame_index - state.last_seen_frame > ttl ||
                    !same_class(state.class_id, class_id)) {
                    continue;
                }
                const int gap = std::max(1, frame_index - state.last_seen_frame);
                const cv::Rect predicted = predict(state, gap);
                const float score = reconnect_match_score(rects[i], predicted, state, gap);
                if (score > 0.30f && motion_is_plausible(rects[i], predicted, state, gap))
                    candidates.push_back(Candidate{score, i, stable_id});
            }
        }

        // When two old lifecycles are equally plausible for a newly allocated
        // raw tracker ID, guessing creates a silent identity swap. Prefer a new
        // lifecycle (and a visible diagnostic counter) over suppressing the
        // wrong person's future alarms.
        std::unordered_map<size_t, std::pair<float, float>> best_two_scores;
        for (const auto& candidate : candidates) {
            auto& [best, second] = best_two_scores[candidate.detection_index];
            if (candidate.score > best) {
                second = best;
                best = candidate.score;
            } else if (candidate.score > second) {
                second = candidate.score;
            }
        }
        std::unordered_set<size_t> ambiguous_detections;
        for (const auto& [detection_index, scores] : best_two_scores) {
            const auto [best, second] = scores;
            if (second > 0.0f && best - second < std::max(0.06f, best * 0.08f)) {
                ambiguous_detections.insert(detection_index);
                ++statistics_.ambiguous_rejected;
            }
        }

        std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
            if (lhs.score != rhs.score) return lhs.score > rhs.score;
            if (lhs.stable_id != rhs.stable_id) return lhs.stable_id < rhs.stable_id;
            return lhs.detection_index < rhs.detection_index;
        });
        std::unordered_set<size_t> assigned_detections;
        for (const auto& candidate : candidates) {
            if (ambiguous_detections.contains(candidate.detection_index) ||
                assigned_detections.contains(candidate.detection_index) ||
                claimed.contains(candidate.stable_id)) continue;
            stable_ids[candidate.detection_index] = candidate.stable_id;
            assigned_detections.insert(candidate.detection_index);
            claimed.insert(candidate.stable_id);
            ++statistics_.reconnected;
            const auto state = states_.find(candidate.stable_id);
            const int gap = state == states_.end()
                ? 1
                : std::max(1, frame_index - state->second.last_seen_frame);
            if (gap == 1) ++statistics_.reconnect_gap_1;
            else if (gap <= 6) ++statistics_.reconnect_gap_2_to_6;
            else ++statistics_.reconnect_gap_over_6;
        }

        // New raw IDs with no conservative reconnect candidate start a new
        // lifecycle. Record aliases only after global assignment is complete.
        for (size_t i = 0; i < count; ++i) {
            if (stable_ids[i] < 0) {
                if (ambiguous_detections.contains(i)) {
                    ++statistics_.allocated_ambiguous;
                } else if (best_two_scores.find(i) == best_two_scores.end()) {
                    ++statistics_.allocated_no_candidate;
                } else {
                    // A viable edge existed but its old lifecycle was already
                    // claimed by a stronger detection in this frame. Treating
                    // both detections as the same person would be an ID switch.
                    ++statistics_.allocated_conflict;
                }
                stable_ids[i] = allocate_stable_id(claimed);
                if (stable_ids[i] >= 0) ++statistics_.allocated;
            }
			if (stable_ids[i] < 0) continue;
            if (raw_ids[i] >= 0 && raw_id_counts[raw_ids[i]] == 1)
                aliases_[raw_ids[i]] = stable_ids[i];
            claimed.insert(stable_ids[i]);
        }

        // Pass 3: refresh geometry only after all assignments are decided.
        for (size_t i = 0; i < count; ++i) {
            if (stable_ids[i] < 0) continue;
            const int class_id = class_at(class_ids, i);
            auto [it, inserted] = states_.try_emplace(
                stable_ids[i], State{stable_ids[i], rects[i], class_id,
                    frame_index, frame_index, 1});
            if (!inserted) update(it->second, rects[i], class_id, frame_index);
        }
        statistics_.live_states = states_.size();
        statistics_.live_aliases = aliases_.size();
        return stable_ids;
    }

    std::vector<int> resolve(const std::vector<int>& raw_ids,
                             const std::vector<cv::Rect>& rects,
                             int frame_index,
                             int max_missing_frames) {
        return resolve(raw_ids, rects, {}, frame_index, max_missing_frames);
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        aliases_.clear();
        states_.clear();
		next_stable_id_ = 1;
        statistics_ = Statistics{};
    }

    Statistics statistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        Statistics snapshot = statistics_;
        snapshot.live_states = states_.size();
        snapshot.live_aliases = aliases_.size();
        return snapshot;
    }

private:
    struct State {
        int stable_id{-1};
        cv::Rect rect{};
        int class_id{-1};
        int last_seen_frame{0};
        int first_seen_frame{0};
        std::uint64_t observations{0};
        float velocity_x{0.0f};
        float velocity_y{0.0f};
        float velocity_w{0.0f};
        float velocity_h{0.0f};
    };

    static int class_at(const std::vector<int>& class_ids, size_t index) {
        return index < class_ids.size() ? class_ids[index] : -1;
    }

    static bool same_class(int lhs, int rhs) {
        return lhs < 0 || rhs < 0 || lhs == rhs;
    }

    static bool motion_is_plausible(const cv::Rect& current,
                                    const cv::Rect& predicted,
                                    const State& state,
                                    int gap) {
        const float center_ratio = rect_center_distance_ratio(current, predicted);
        const float size_ratio = rect_size_ratio(current, state.rect);
        // Permit a wider gate for a short occlusion, but reject teleports and
        // large scale jumps that commonly bind a new nearby target to an old ID.
        const float max_center_ratio = 0.90f + 0.20f * static_cast<float>(std::min(gap, 3));
        return center_ratio <= max_center_ratio && size_ratio >= 0.30f;
    }

    static bool alias_motion_is_plausible(const cv::Rect& current,
                                           const cv::Rect& predicted,
                                           const State& state,
                                           int gap) {
        const float center_ratio = std::min(
            rect_center_distance_ratio(current, predicted),
            rect_center_distance_ratio(current, state.rect));
        const float size_ratio = rect_size_ratio(current, state.rect);
        // Existing tracker IDs are stronger evidence than a geometry-only
        // reconnect. The gate is intentionally lenient, but still rejects raw-ID
        // reuse across unrelated regions or catastrophic scale changes.
        const float max_center_ratio = 2.50f + 0.25f * static_cast<float>(std::min(gap, 6));
        return center_ratio <= max_center_ratio && size_ratio >= 0.15f;
    }

    static float alias_match_score(const cv::Rect& current,
                                   const cv::Rect& predicted,
                                   const State& state) {
        const float center_ratio = std::min(
            rect_center_distance_ratio(current, predicted),
            rect_center_distance_ratio(current, state.rect));
        const float size_ratio = rect_size_ratio(current, state.rect);
        const float iou = std::max(rect_iou(current, predicted), rect_iou(current, state.rect));
        return 4.0f + iou + size_ratio * 0.25f - std::min(center_ratio, 3.0f) * 0.10f;
    }

    static float reconnect_match_score(const cv::Rect& current,
                                       const cv::Rect& predicted,
                                       const State& state,
                                       int gap) {
        float score = std::max(
            fallback_match_score(current, predicted),
            fallback_match_score(current, state.rect));
        if (score > 0.30f) return score;

        // A small/far target can move by more than one box width between two
        // processed frames, yielding zero IoU even though the motion is
        // continuous. Bridge only a bounded sub-second gap and require strong
        // scale agreement. Six processed frames covers decoder jitter and a
        // brief pedestrian occlusion at 25/30fps, while the global one-to-one
        // assignment and ambiguity gate below still prefer a fresh lifecycle
        // whenever two people are plausible.
		if (gap > 6) return 0.0f;
        const float center_ratio = std::min(
            rect_center_distance_ratio(current, predicted),
            rect_center_distance_ratio(current, state.rect));
        const float size_ratio = rect_size_ratio(current, state.rect);
        constexpr float max_center_ratio = 1.35f;
        if (center_ratio > max_center_ratio || size_ratio < 0.45f) return 0.0f;
        const float center_score = 1.0f - center_ratio / max_center_ratio;
        return 0.31f + center_score * 0.24f + size_ratio * 0.20f;
    }

    static cv::Rect predict(const State& state, int gap) {
        const float horizon = static_cast<float>(std::clamp(gap, 1, 3));
        return cv::Rect(
            cvRound(static_cast<float>(state.rect.x) + state.velocity_x * horizon),
            cvRound(static_cast<float>(state.rect.y) + state.velocity_y * horizon),
            std::max(1, cvRound(static_cast<float>(state.rect.width) + state.velocity_w * horizon)),
            std::max(1, cvRound(static_cast<float>(state.rect.height) + state.velocity_h * horizon)));
    }

    static void update(State& state, const cv::Rect& rect, int class_id, int frame_index) {
        const int gap = std::max(1, frame_index - state.last_seen_frame);
        constexpr float alpha = 0.40f;
        const float inv_gap = 1.0f / static_cast<float>(gap);
        const float dx = static_cast<float>(rect.x - state.rect.x) * inv_gap;
        const float dy = static_cast<float>(rect.y - state.rect.y) * inv_gap;
        const float dw = static_cast<float>(rect.width - state.rect.width) * inv_gap;
        const float dh = static_cast<float>(rect.height - state.rect.height) * inv_gap;
        state.velocity_x = state.velocity_x * (1.0f - alpha) + dx * alpha;
        state.velocity_y = state.velocity_y * (1.0f - alpha) + dy * alpha;
        state.velocity_w = state.velocity_w * (1.0f - alpha) + dw * alpha;
        state.velocity_h = state.velocity_h * (1.0f - alpha) + dh * alpha;
        state.rect = rect;
        // Unknown labels must not erase an established class. A concrete class
        // change is accepted only through a new lifecycle association.
        if (state.class_id < 0 && class_id >= 0) state.class_id = class_id;
        state.last_seen_frame = frame_index;
        ++state.observations;
    }

    void cleanup(int frame_index, int ttl) {
        // mutex_ is held by resolve().
        for (auto it = states_.begin(); it != states_.end();) {
            const State& state = it->second;
            if (frame_index - state.last_seen_frame <= ttl) {
                ++it;
                continue;
            }
            ++statistics_.expired;
            if (state.observations <= 1) ++statistics_.expired_single_observation;
            if (state.observations <= 3) ++statistics_.expired_short_lifecycle;
            it = states_.erase(it);
        }
        std::erase_if(aliases_, [&](const auto& item) {
            return !states_.contains(item.second);
        });
    }

    int allocate_stable_id(const std::unordered_set<int>& claimed) {
        // mutex_ is held by resolve().
        // IDs are scoped to one pipeline/task session. Keep them compact for OSD
        // and never expose BotSort's process-global static counter as business ID.
        for (int attempts = 0; attempts < kFallbackTrackBase - 1; ++attempts) {
            if (next_stable_id_ <= 0 || next_stable_id_ >= kFallbackTrackBase)
                next_stable_id_ = 1;
            const int candidate = next_stable_id_++;
            if (!states_.contains(candidate) && !claimed.contains(candidate)) return candidate;
        }
		// Exhaustion is practically unreachable in the short resolver window.
		// Returning -1 is safer than colliding with the reserved fallback range.
        return -1;
    }

    mutable std::mutex mutex_;
    std::unordered_map<int, int> aliases_;
    std::unordered_map<int, State> states_;
    int next_stable_id_{1};
    Statistics statistics_{};
};

inline int stable_id_stitch_frames(int configured_frame_rate, int lifecycle_ttl_frames) {
    // Keep one nominal second of lifecycle history. Geometry-only reconnects
    // become stricter as the gap grows, while this longer retention lets a raw
    // tracker ID recover after brief occlusion without allocating a new business
    // ID. Keep an absolute cap for misconfigured frame rates.
    return std::max(1, std::min({lifecycle_ttl_frames,
                                std::max(4, configured_frame_rate),
                                30}));
}

template <typename TrackList>
inline int best_fallback_track_match(const cv::Rect& current,
                                     const TrackList& tracks,
                                     const std::vector<bool>& used,
                                     size_t track_count) {
	const size_t n = std::min({track_count, tracks.size(), used.size()});
	float best_score = 0.30f;
	int best_index = -1;
	for (size_t i = 0; i < n; ++i) {
		if (used[i]) continue;
		const float score = fallback_match_score(current, tracks[i].rect);
		if (score > best_score) {
			best_score = score;
			best_index = static_cast<int>(i);
		}
	}
	return best_index;
}

inline int best_rect_match(const cv::Rect& current,
                           const std::vector<cv::Rect>& rects,
                           const std::vector<bool>& used,
                           size_t rect_count) {
	const size_t n = std::min({rect_count, rects.size(), used.size()});
	float best_score = 0.30f;
	int best_index = -1;
	for (size_t i = 0; i < n; ++i) {
		if (used[i]) continue;
		const float score = fallback_match_score(current, rects[i]);
		if (score > best_score) {
			best_score = score;
			best_index = static_cast<int>(i);
		}
	}
	return best_index;
}

inline cv::Mat make_tracker_frame(int width, int height) {
	// All AXVideoFrame adapters currently run BotSort in detection-only mode:
	// GMC and ReID are disabled and no real image pixels are available here.
	// Allocating an uninitialised width*height Mat on every inference therefore
	// has no semantic value.  It also creates multi-megabyte cross-thread Mat
	// ownership churn; under many independent tasks this exposed allocator
	// corruption while BotSort replaced its previous input vector.
	//
	// BotSort explicitly supports an empty frame in detection-only mode and uses
	// the already-clipped detector boxes unchanged.  Preserve the parameters in
	// this API for source compatibility, but keep the hot path allocation-free.
	(void)width;
	(void)height;
	return cv::Mat{};
}

inline cv::Rect rect_from_tlwh(const std::vector<float>& tlwh) {
	if (tlwh.size() < 4) return cv::Rect{};
	return cv::Rect(cvRound(tlwh[0]), cvRound(tlwh[1]), cvRound(tlwh[2]), cvRound(tlwh[3]));
}

template <typename TrackList, typename OnAssigned>
inline int assign_tracker_ids_by_iou(const TrackList& tracks,
                                     const std::vector<cv::Rect>& rects,
                                     std::vector<int>& track_ids,
                                     OnAssigned on_assigned,
                                     float min_iou = 0.30f) {
	const size_t n = std::min(rects.size(), track_ids.size());
	std::vector<bool> used(n, false);
	int assigned = 0;
	for (const auto& track : tracks) {
		if (track.track_id_ < 0) continue;

		int best_index = -1;
		// BotSort already records the exact input detection selected by LAPJV.
		// Preserve that association; geometric matching is only compatibility
		// fallback for old libraries or malformed indices.
		if (track.det_index_ >= 0 && track.det_index_ < static_cast<int>(n)
		    && !used[track.det_index_] && track_ids[track.det_index_] < 0) {
			best_index = track.det_index_;
		}

		float best_iou = min_iou;
		if (best_index < 0 && track.track_tlwh_.size() >= 4) {
			const cv::Rect trk_rect = rect_from_tlwh(track.track_tlwh_);
			if (trk_rect.area() > 0) {
				for (size_t i = 0; i < n; ++i) {
					if (used[i] || track_ids[i] >= 0) continue;
					const float iou = rect_iou(trk_rect, rects[i]);
					if (iou > best_iou) {
						best_iou = iou;
						best_index = static_cast<int>(i);
					}
				}
			}
		}

		if (best_index >= 0) {
			track_ids[best_index] = track.track_id_;
			used[best_index] = true;
			on_assigned(static_cast<size_t>(best_index), track);
			++assigned;
		}
	}
	return assigned;
}

template <typename TrackList>
inline int assign_tracker_ids_by_iou(const TrackList& tracks,
                                     const std::vector<cv::Rect>& rects,
                                     std::vector<int>& track_ids,
                                     float min_iou = 0.30f) {
	return assign_tracker_ids_by_iou(
	    tracks, rects, track_ids, [](size_t, const auto&) {}, min_iou);
}

}  // namespace jdk_tracking

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif
