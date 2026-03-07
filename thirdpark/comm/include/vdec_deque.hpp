#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>

#ifndef __VDEC_DEQUE_H__
#define __VDEC_DEQUE_H__

enum class QueueMode {
	Default,	// mode 1 handle packets one by one without packet loss
	Block,		// mode 2 blocking mode blocking producers when the queue is full
	DropOldest	// mode 3 discards old data when the queue is full and processes the latest frame
};

template <typename T>
class safe_queue {
public:
	safe_queue(size_t max_capacity = 3 /*std::numeric_limits<size_t>::max()*/, QueueMode mode = QueueMode::DropOldest)
		: max_capacity_(max_capacity), mode_(mode) {}
	// disable copy and assignment operations
	safe_queue(const safe_queue&)			 = delete;
	safe_queue& operator=(const safe_queue&) = delete;

	// putting data into a queue producer call
	void push(const T& value, std::atomic<bool>* thread_alive /* = nullptr*/, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
		{
			std::unique_lock<std::mutex> lock(mutex_);
			if (!is_running_.load() || (thread_alive && !thread_alive->load())) {
				return;	 // if the queue stops running it goes back
			}
			if (mode_ == QueueMode::Block) {
				if (!cond_var_not_full_.wait_for(lock, timeout, [this, thread_alive]() {
						return queue_.size() < max_capacity_ || !is_running_.load() || (thread_alive && !thread_alive->load());
					})) {
					if (!is_running_.load() || (thread_alive && !thread_alive->load())) {
						return;
					}
					throw std::runtime_error("Timeout while waiting to push data into queue");
				};
			} else if (mode_ == QueueMode::DropOldest) {
				if (queue_.size() >= max_capacity_ && !queue_.empty()) {
					queue_.pop();  // discard the oldest data
				}
			}
			if (is_running_.load() && (!thread_alive || thread_alive->load())) {
				queue_.push(value);
			}
		}
		cond_var_not_empty_.notify_one();
	}

	// fetch data from a queue consumer call
	T get(std::atomic<bool>* thread_alive /* = nullptr*/, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
		T value;
		{
			// printf("@@@@@@@@@@@@@---->[%s], %s,%d, thread_alive: %s\r\n", node_name_.data(), __FUNCTION__, __LINE__,
			//    thread_alive ? (thread_alive->load() ? "true" : "false") : "nullptr");
			std::unique_lock<std::mutex> lock(mutex_);
			// cond_var_.wait(lock, [this]() { return !queue_.empty(); });
			// T value = queue_.front();
			// queue_.pop();
			// printf("---->[%s], %s,%d\r\n", node_name_.data(), __FUNCTION__, __LINE__);
			if (!cond_var_not_empty_.wait_for(lock, timeout, [this, thread_alive]() {
					return !queue_.empty() || !is_running_.load() || (thread_alive && !thread_alive->load());
				})) {
				// printf(")))is_running_:[%s] %d,%d\r\n", node_name_.data(), (int)is_running_.load(), (int)queue_.empty());
				fflush(stdout);
				// if the queue is stopped or empty the default value is returned
				if (!is_running_.load() || queue_.empty() || (thread_alive && !thread_alive->load())) {
					return nullptr;
				}
				return nullptr;
			};
			// printf("---->[%s], %s,%d\r\n", node_name_.data(), __FUNCTION__, __LINE__);
			// T value = queue_.front();
			// queue_.pop();
			if ((!is_running_) && queue_.empty())
				return nullptr;
			if (thread_alive && !thread_alive->load())
				return nullptr;

			// printf("---->[%s], %s,%d\r\n", node_name_.data(), __FUNCTION__, __LINE__);
			if (mode_ == QueueMode::Default || mode_ == QueueMode::Block) {
				// mode 1 handle them one by one
				value = queue_.front();
				queue_.pop();
			} else if (mode_ == QueueMode::DropOldest) {
				// mode 2 process the latest frame and discard the old frame
				while (queue_.size() > 1) {
					queue_.pop();
				}
				value = queue_.back();
				queue_.pop();
			}
			// printf("---->[%s], %s,%d\r\n", node_name_.data(), __FUNCTION__, __LINE__);
		}
		if (mode_ == QueueMode::Block) {
			cond_var_not_full_.notify_one();
		}
		// printf("---->[%s], %s,%d\r\n", node_name_.data(), __FUNCTION__, __LINE__);
		return value;
	}

	// attempts to get data from the queue non blocking returns false immediately if there is no data
	bool try_get(T& value) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (queue_.empty()) {
			return false;
		}
		if (mode_ == QueueMode::DropOldest) {
			value = queue_.back();
			queue_.pop();
		} else {
			while (queue_.size() > 1) {
				queue_.pop();
			}
			value = queue_.front();
			queue_.pop();
		}

		return true;
	}

	// gets the current size of the queue
	size_t size() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return queue_.size();
	}

	// check if the queue is empty
	bool empty() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return queue_.empty();
	}

	// set the maximum capacity of the queue
	void set_max_capacity(size_t max_capacity) {
		std::lock_guard<std::mutex> lock(mutex_);
		max_capacity_ = max_capacity;
	}

	// get the maximum capacity of the queue
	size_t get_max_capacity() {
		std::lock_guard<std::mutex> lock(mutex_);
		return max_capacity_;
	}

	// set the mode of the queue
	void set_mode(QueueMode mode) {
		std::lock_guard<std::mutex> lock(mutex_);
		mode_ = mode;
	}
	void set_name(std::string name) {
		std::lock_guard<std::mutex> lock(mutex_);
		node_name_ = name;
	}

	// get the mode of the queue
	QueueMode get_mode() {
		std::lock_guard<std::mutex> lock(mutex_);
		return mode_;
	}
	void shutdown() {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			fflush(stdout);
			is_running_.store(false);  // set a stop sign
			while (!queue_.empty()) {
				queue_.pop();  // clear the queue
			}
		}
		cond_var_not_empty_.notify_all();  // wake up all waiting threads
		cond_var_not_full_.notify_all();
	}

private:
	mutable std::mutex		mutex_;
	std::queue<T>			queue_;
	std::condition_variable cond_var_not_empty_;
	std::condition_variable cond_var_not_full_;
	size_t					max_capacity_;
	QueueMode				mode_;
	std::string				node_name_;
	// bool					is_running_{true};
	std::atomic<bool> is_running_{true};
};

#endif
