#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <deque>
#include <optional>
#include <vector>

namespace jdk_tracking {

// Estimates the cadence of frames that actually completed detector processing.
// The median rejects short scheduling/inference jitter, while a discontinuity
// starts a fresh observation window instead of poisoning the next estimate.
class ObservedProcessedFps {
public:
	using clock = std::chrono::steady_clock;
	using time_point = clock::time_point;

	struct Observation {
		double fps{0.0};
		bool reliable{false};
		bool gap_reset{false};
		std::size_t interval_count{0};
	};

	Observation observe() { return observe(clock::now()); }

	Observation observe(time_point now) {
		if (!have_last_) {
			start_window_(now);
			return {};
		}

		const double interval_seconds =
			std::chrono::duration<double>(now - last_).count();
		if (!(interval_seconds > 0.0)) return current_(now, false);

		if (is_gap_(interval_seconds)) {
			start_window_(now);
			Observation result;
			result.gap_reset = true;
			return result;
		}

		last_ = now;
		intervals_.push_back(interval_seconds);
		if (intervals_.size() > kMaxIntervals) intervals_.pop_front();
		return current_(now, false);
	}

	void reset() {
		intervals_.clear();
		have_last_ = false;
		window_started_ = time_point{};
		last_ = time_point{};
	}

private:
	static constexpr std::size_t kMaxIntervals = 31;
	// Never let a single slow startup inference redefine BotSort's motion clock.
	// Five intervals still converge quickly (about 0.17s at 30fps, 0.4s at
	// 12.5fps and 1s at 5fps) while rejecting one-off model warm-up stalls.
	static constexpr std::size_t kMinimumEvidenceIntervals = 5;
	static constexpr std::size_t kMinIntervals = 8;
	static constexpr double kMinWindowSeconds = 0.5;
	static constexpr double kAbsoluteGapSeconds = 2.0;

	void start_window_(time_point now) {
		intervals_.clear();
		window_started_ = now;
		last_ = now;
		have_last_ = true;
	}

	double median_interval_() const {
		if (intervals_.empty()) return 0.0;
		std::vector<double> sorted(intervals_.begin(), intervals_.end());
		std::sort(sorted.begin(), sorted.end());
		const std::size_t middle = sorted.size() / 2;
		if ((sorted.size() & 1U) != 0U) return sorted[middle];
		return (sorted[middle - 1] + sorted[middle]) * 0.5;
	}

	bool is_gap_(double interval_seconds) const {
		if (interval_seconds >= kAbsoluteGapSeconds) return true;
		if (intervals_.size() < 3) return false;
		const double median = median_interval_();
		return median > 0.0 &&
			interval_seconds > std::max(0.25, median * 4.0);
	}

	Observation current_(time_point now, bool gap_reset) const {
		Observation result;
		result.gap_reset = gap_reset;
		result.interval_count = intervals_.size();
		if (intervals_.empty()) return result;

		const double window_seconds =
			std::chrono::duration<double>(now - window_started_).count();
		result.reliable = intervals_.size() >= kMinimumEvidenceIntervals &&
			(intervals_.size() >= kMinIntervals ||
			 window_seconds >= kMinWindowSeconds);
		if (!result.reliable) return result;

		const double median = median_interval_();
		if (median > 0.0) result.fps = std::clamp(1.0 / median, 1.0, 120.0);
		return result;
	}

	std::deque<double> intervals_;
	time_point window_started_{};
	time_point last_{};
	bool have_last_{false};
};

// Measures absence only across continuously observed processed frames. A media
// or scheduler gap is not evidence that the object left the scene, so the first
// frame after a gap starts a fresh absence window instead of expiring state.
class ObservedAbsence {
public:
	using clock = ObservedProcessedFps::clock;
	using time_point = ObservedProcessedFps::time_point;

	void observe(time_point now, bool present, bool continuity_broken = false) {
		double interval_seconds = 0.0;
		if (last_observation_ && now >= *last_observation_) {
			interval_seconds = std::chrono::duration<double>(
				now - *last_observation_).count();
		}
		last_observation_ = now;

		if (continuity_broken || !continuous_interval_(interval_seconds)) {
			intervals_.clear();
			observed_missing_ = clock::duration::zero();
			missing_active_ = false;
			if (present || continuity_broken) return;
		}
		if (interval_seconds > 0.0) remember_interval_(interval_seconds);

		if (present) {
			observed_missing_ = clock::duration::zero();
			missing_active_ = false;
			return;
		}
		if (!missing_active_) {
			missing_active_ = true;
			return;
		}

		const double observed_step = representative_interval_(interval_seconds);
		if (observed_step > 0.0) {
			observed_missing_ += std::chrono::duration_cast<clock::duration>(
				std::chrono::duration<double>(observed_step));
		}
	}

	bool expired(time_point, clock::duration required) const {
		return missing_active_ && observed_missing_ >= required;
	}

	void reset() {
		intervals_.clear();
		last_observation_.reset();
		observed_missing_ = clock::duration::zero();
		missing_active_ = false;
	}
	bool active() const { return missing_active_; }

private:
	static constexpr std::size_t kMaxIntervals = 15;

	double median_interval_() const {
		if (intervals_.empty()) return 0.0;
		std::vector<double> sorted(intervals_.begin(), intervals_.end());
		std::sort(sorted.begin(), sorted.end());
		const std::size_t middle = sorted.size() / 2;
		if ((sorted.size() & 1U) != 0U) return sorted[middle];
		return (sorted[middle - 1] + sorted[middle]) * 0.5;
	}

	bool continuous_interval_(double interval_seconds) const {
		if (!(interval_seconds > 0.0)) return !last_observation_.has_value();
		if (interval_seconds >= 2.0) return false;
		if (intervals_.size() < 3) return interval_seconds <= 1.0;
		const double median = median_interval_();
		return median > 0.0 &&
			interval_seconds <= std::max(0.25, median * 4.0);
	}

	void remember_interval_(double interval_seconds) {
		intervals_.push_back(interval_seconds);
		if (intervals_.size() > kMaxIntervals) intervals_.pop_front();
	}

	double representative_interval_(double fallback) const {
		if (intervals_.size() >= 3) return median_interval_();
		return std::clamp(fallback, 0.0, 0.25);
	}

	std::deque<double> intervals_;
	std::optional<time_point> last_observation_;
	clock::duration observed_missing_{clock::duration::zero()};
	bool missing_active_{false};
};

}  // namespace jdk_tracking
