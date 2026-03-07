#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
// rmwei add ..
class SnapshotCleaner {
	public:
		static SnapshotCleaner& get() {
			static SnapshotCleaner instance;
			return instance;
		}

		// asynchronous cleanup tasks,trigger immediately
		void clean_async(const std::string& dir_path,
						 int days_to_keep,
						 uintmax_t max_total_size,
						 const std::function<void(const std::string&)>& on_deleted = {}) {
			std::lock_guard lock(mutex_);
			if (running_) return;  // avoid concurrent duplicate cleanups

			running_ = true;
			std::thread([this, dir_path, days_to_keep, max_total_size, on_deleted] {
				this->cleanup_impl(dir_path, days_to_keep, max_total_size, on_deleted);
				std::lock_guard lock(mutex_);
				running_ = false;
				cv_.notify_all();
			}).detach();
		}

	// wait for the task to complete(optional)
	void wait() {
		std::unique_lock lock(mutex_);
		cv_.wait(lock, [this] { return !running_; });
	}

private:
	SnapshotCleaner() = default;
	std::mutex				mutex_;
	std::condition_variable cv_;
	std::atomic<bool>		running_{false};

		void cleanup_impl(const std::string& dir_path,
						  int days_to_keep,
						  uintmax_t max_total_size,
						  const std::function<void(const std::string&)>& on_deleted) {
			using namespace std::filesystem;
			using namespace std::chrono;

		struct FileInfo {
			path					 file_path;
			system_clock::time_point mod_time;
			uintmax_t				 file_size;
		};

		std::vector<FileInfo> jpg_files;
		uintmax_t			  total_size = 0;
		const auto			  now		 = system_clock::now();
		const auto			  cutoff	 = now - hours(24 * days_to_keep);
		std::error_code		  ec;

		for (const auto& entry : directory_iterator(dir_path, ec)) {
			if (ec || !entry.is_regular_file()) continue;
			const auto& path = entry.path();
			if (path.extension() != ".jpg") continue;

			auto ftime = last_write_time(path, ec);
			if (ec) continue;

			auto sctp = time_point_cast<system_clock::duration>(
				ftime - decltype(ftime)::clock::now() + system_clock::now());

			uintmax_t fsize = std::filesystem::file_size(path, ec);
			if (ec) continue;

			total_size += fsize;
			jpg_files.push_back({path, sctp, fsize});
		}

		std::sort(jpg_files.begin(), jpg_files.end(),
				  [](const FileInfo& a, const FileInfo& b) {
					  return a.mod_time < b.mod_time;
				  });

			// 1. delete expired ones
			for (const auto& file : jpg_files) {
				if (file.mod_time >= cutoff) break;
				if (remove(file.file_path, ec)) {
					std::cout << "[SnapshotCleaner] Deleted expired: " << file.file_path << "\n";
					if (on_deleted) on_deleted(file.file_path.string());
					total_size -= file.file_size;
				}
			}

			// 2.delete the one that is out of space（the oldest starts）
			for (const auto& file : jpg_files) {
				if (total_size <= max_total_size) break;
				if (exists(file.file_path, ec) && remove(file.file_path, ec)) {
					std::cout << "[SnapshotCleaner] Deleted for space: " << file.file_path << "\n";
					if (on_deleted) on_deleted(file.file_path.string());
					total_size -= file.file_size;
				}
			}
		}
};
