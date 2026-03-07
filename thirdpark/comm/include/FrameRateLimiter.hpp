#pragma once
#include <chrono>
#include <thread>

class FrameRateLimiter {
public:
	explicit FrameRateLimiter(int max_fps)
		: frame_interval_(1000 / max_fps),
		  last_time_(std::chrono::steady_clock::now()) {}

	void throttle() {
		auto now	 = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time_);
		if (elapsed.count() < frame_interval_) {
			std::this_thread::sleep_for(std::chrono::milliseconds(frame_interval_ - elapsed.count()));
		}
		last_time_ = std::chrono::steady_clock::now();
	}

private:
	int									  frame_interval_;	// milliseconds between frames
	std::chrono::steady_clock::time_point last_time_;
};