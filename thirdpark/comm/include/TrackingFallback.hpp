#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <opencv2/core/core.hpp>

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
	if (width <= 0 || height <= 0) return cv::Mat{};
	return cv::Mat(height, width, CV_8UC1);
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
		float best_iou = min_iou;
		if (track.track_tlwh_.size() >= 4) {
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

		if (best_index < 0 && track.track_tlwh_.size() < 4 && track.det_index_ >= 0
		    && track.det_index_ < static_cast<int>(n) && !used[track.det_index_]
		    && track_ids[track.det_index_] < 0) {
			best_index = track.det_index_;
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
