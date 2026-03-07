#pragma once
#include <chrono>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <vector>

template <typename T>
class TimestampedRingBuffer {
public:
	using TimePoint = std::chrono::steady_clock::time_point;

	struct Item {
		TimePoint		   timestamp;
		std::shared_ptr<T> data;
	};

	explicit TimestampedRingBuffer(size_t capacity)
		: capacity_(capacity), buffer_(capacity) {}

	void stop() {
		std::unique_lock lock(mutex_);
		stopped_ = true;
		cv_.notify_all();
	}

	~TimestampedRingBuffer() {
		stop();
	}

	void push(std::shared_ptr<T> item) {
		std::unique_lock lock(mutex_);
		TimePoint		 now  = std::chrono::steady_clock::now();
		buffer_[write_index_] = {now, item};
		write_index_		  = (write_index_ + 1) % capacity_;
		if (size_ < capacity_) ++size_;
		cv_.notify_all();
	}

	std::optional<Item> wait_for_new_frame(TimePoint& last_time, std::chrono::milliseconds timeout = std::chrono::milliseconds(200)) {
		TimePoint target_time;
		{
			std::shared_lock lock(mutex_);
			if (size_ > 0) {
				const Item& latest = buffer_[(write_index_ + capacity_ - 1) % capacity_];
				if (latest.timestamp > last_time) {
					last_time = latest.timestamp;
					return latest;
				}
				target_time = latest.timestamp;
			}
		}

		std::unique_lock lock(mutex_);
		bool			 success = cv_.wait_for(lock, timeout, [&]() {
			if (stopped_) return true;
			if (size_ == 0) return false;
			const Item& latest = buffer_[(write_index_ + capacity_ - 1) % capacity_];
			return latest.timestamp > last_time;
		});
		if (stopped_) return std::nullopt;
		if (!success) return std::nullopt;

		const Item& latest = buffer_[(write_index_ + capacity_ - 1) % capacity_];
		last_time		   = latest.timestamp;
		return latest;
	}

private:
	size_t						capacity_;
	mutable std::shared_mutex	mutex_;
	std::vector<Item>			buffer_;
	std::condition_variable_any cv_;
	size_t						write_index_ = 0;
	size_t						size_		 = 0;
	bool						stopped_	 = false;
};
