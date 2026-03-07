#pragma once
#include <dlfcn.h>
#include <liburing.h>

#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <json.hpp>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include "sdk_interface.hpp"

const std::vector<int>& ax_device_ids();

struct PluginInfo {
	std::string	   name;
	std::string	   path;
	nlohmann::json config;
	void*		   handle					= nullptr;
	void (*cleanup_func)(SDKInterface* sdk) = nullptr;
};

class ThreadPool {
public:
	ThreadPool(size_t n) : stop(false) {
		for (size_t i = 0; i < n; ++i) {
			workers.emplace_back([this] {
				for (;;) {
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(this->mtx);
						this->cv.wait(lock, [this] { return stop || !tasks.empty(); });
						if (stop && tasks.empty()) return;
						task = std::move(tasks.front());
						tasks.pop();
					}
					task();
				}
			});
		}
	}
	void shutdown_and_wait() {
		bool expected = false;
		if (!has_shutdown_.compare_exchange_strong(expected, true)) return;

		{
			std::lock_guard<std::mutex> lk(mtx);
			stop = true;
		}
		cv.notify_all();
		for (auto& w : workers) {
			if (w.joinable()) w.join();
		}
	}
	~ThreadPool() {
		shutdown_and_wait();
	}
	template <class F>
	void enqueue(F&& f) {
		{
			std::lock_guard<std::mutex> lock(mtx);
			if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");
			tasks.emplace(std::forward<F>(f));
		}
		cv.notify_one();
	}
	ThreadPool(const ThreadPool&)			 = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	ThreadPool(ThreadPool&&)			= delete;
	ThreadPool& operator=(ThreadPool&&) = delete;

private:
	std::atomic<bool>				  has_shutdown_{false};
	std::vector<std::thread>		  workers;
	std::queue<std::function<void()>> tasks;
	std::mutex						  mtx;
	std::condition_variable			  cv;
	bool							  stop;
};

class PluginLoader {
public:
	PluginLoader(size_t threads = std::thread::hardware_concurrency(), unsigned queue_depth = 256)
		: pool_(threads) {
		// if (io_uring_queue_init(queue_depth, &ring_, 0) < 0) {
		// 	std::cerr << "io_uring init failed" << std::endl;
		// 	std::terminate();
		// }
	}
	~PluginLoader();  // automatic cleanup of plugins

	bool load_all_plugins(const std::string& plugin_root, SDKInterface* sdk);
	void loadSingle(const std::string pluginPath);

	const std::vector<PluginInfo>& get_loaded_plugins() const { return plugins_; }

	nlohmann::json get_component_structure() const {
		return component_structure_;
	}

private:
	std::vector<PluginInfo> plugins_;
	SDKInterface*			sdk_ = nullptr;

	nlohmann::json component_structure_ = nlohmann::json::array();
	//

	std::unordered_map<std::string, std::string> cache_;
	std::mutex									 cwd_m_;
	// io_uring									 ring_;
	ThreadPool	pool_;
	bool		support_local_ = false;
	std::string target_soc_{};
};