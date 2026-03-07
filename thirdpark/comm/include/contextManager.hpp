#pragma once
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
// rmwei add .
#include "axcl.h"

class AXCLLifecycle {
public:
	static AXCLLifecycle& instance() EXPORT_VISIBILITY;

	bool init_once(int device_id);
	void finalize_once(int device_id);
	void finalize_all();
	bool is_finalized(int device_id) const;

private:
	AXCLLifecycle() = default;
	~AXCLLifecycle() noexcept EXPORT_VISIBILITY;
	AXCLLifecycle(const AXCLLifecycle&)			   = delete;
	AXCLLifecycle& operator=(const AXCLLifecycle&) = delete;

	std::unordered_set<int> inited_devices_;
	mutable std::mutex		mutex_;
};

class AXCLIvpsLifecycle {
public:
	static AXCLIvpsLifecycle& instance() EXPORT_VISIBILITY;

	bool init_once(int device_id);
	void deinit_once(int device_id);
	bool is_finalized(int device_id) const;

private:
	AXCLIvpsLifecycle() = default;
	~AXCLIvpsLifecycle() noexcept EXPORT_VISIBILITY;
	std::unordered_set<int> inited_devices_;
	mutable std::mutex		mutex_;
};

class AXCLVencLifecycle {
public:
	static AXCLVencLifecycle& instance() EXPORT_VISIBILITY;

	bool init_once(int device_id, const AX_VENC_MOD_ATTR_T* venc_opt = nullptr);
	void deinit_once(int device_id);
	bool is_finalized(int device_id) const;

private:
	AXCLVencLifecycle() = default;
	~AXCLVencLifecycle() noexcept EXPORT_VISIBILITY;
	std::unordered_set<int> inited_devices_;
	mutable std::mutex		mutex_;
};

class AXCLVdecLifecycle {
public:
	static AXCLVdecLifecycle& instance() EXPORT_VISIBILITY;

	bool init_once(int device_id, const AX_VDEC_MOD_ATTR_T* vdec_opt = nullptr);
	void deinit_once(int device_id);
	bool is_finalized(int device_id) const;

private:
	AXCLVdecLifecycle() = default;
	~AXCLVdecLifecycle() noexcept EXPORT_VISIBILITY;
	std::unordered_set<int> inited_devices_;
	mutable std::mutex		mutex_;
};

class EXPORT_VISIBILITY AXCLEngineLifecycle {
public:
	static AXCLEngineLifecycle& instance() EXPORT_VISIBILITY;

	bool init_once(int device_id, const AX_ENGINE_NPU_ATTR_T* npu_opt = nullptr);
	void deinit_once(int device_id);
	bool is_finalized(int device_id) const;

private:
	AXCLEngineLifecycle() = default;
	~AXCLEngineLifecycle() noexcept EXPORT_VISIBILITY;
	std::unordered_set<int> inited_devices_;
	mutable std::mutex		mutex_;
};

class AXCLContextManager {
public:
	static AXCLContextManager& instance() EXPORT_VISIBILITY;

	axclrtContext get_context(int device_index);
	void		  bind(int device_id);
	void		  destroy_thread_contexts_if_unused();
	void		  destroy_all();

private:
	std::mutex														  mutex_;
	std::unordered_map<pid_t, std::unordered_map<int, axclrtContext>> thread_context_map_{};
	std::unordered_map<int, int>									  deviceId_map{};  // Device ID mapping table, key: device_id, value: id index
};

class AXWatchdog {
public:
	AXWatchdog(const std::string& name, int timeout_ms)
		: name_(name), timeout_(timeout_ms) {
		last_update_ = std::chrono::steady_clock::now();
	}

	void kick() {
		last_update_ = std::chrono::steady_clock::now();
	}

	void check() {
		auto now	  = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_).count();
		if (duration > timeout_) {
			std::cerr << "[AXWatchdog] ⚠️ thread " << name_ << " may be stuck for " << duration << " ms" << std::endl;
		}
	}

private:
	std::string							  name_;
	int									  timeout_;
	std::chrono::steady_clock::time_point last_update_;
};
