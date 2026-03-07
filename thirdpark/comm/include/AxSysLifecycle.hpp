// AxSysLifecycle.hpp
#pragma once
#include <atomic>
#include <iostream>
#include <mutex>

#include "ax_engine_api.h"
#include "ax_vdec_api.h"
#include "ax_venc_api.h"

class AXSysLifecycle {
public:
	static AXSysLifecycle& instance() EXPORT_VISIBILITY;

	bool init_once();
	void deinit_once();
	bool is_finalized() const;

private:
	AXSysLifecycle();
	~AXSysLifecycle() noexcept EXPORT_VISIBILITY;
	std::atomic_bool inited_{false};
	std::mutex		 mutex_;
};

class AXIvpsLifecycle {
public:
	static AXIvpsLifecycle& instance() EXPORT_VISIBILITY;

	bool init_once();
	void deinit_once();
	bool is_finalized() const;

private:
	AXIvpsLifecycle();
	~AXIvpsLifecycle() noexcept EXPORT_VISIBILITY;
	std::atomic_bool inited_{false};
	std::mutex		 mutex_;
};

class EXPORT_VISIBILITY AXEngineLifecycle {
public:
	static AXEngineLifecycle& instance() EXPORT_VISIBILITY;

	bool init_once(const AX_ENGINE_NPU_ATTR_T* npu_opt = nullptr);
	void deinit_once();
	bool is_finalized() const;

private:
	AXEngineLifecycle();
	~AXEngineLifecycle() noexcept EXPORT_VISIBILITY;
	std::atomic_bool inited_{false};
	std::mutex		 mutex_;
};

class AXVencLifecycle {
public:
	static AXVencLifecycle& instance() EXPORT_VISIBILITY;

	bool init_once(const AX_VENC_MOD_ATTR_T* venc_opt = nullptr);
	void deinit_once();
	bool is_finalized() const;

private:
	AXVencLifecycle();
	~AXVencLifecycle() noexcept EXPORT_VISIBILITY;
	std::atomic_bool inited_{false};
	std::mutex		 mutex_;
};

class AXVdecLifecycle {
public:
	static AXVdecLifecycle& instance() EXPORT_VISIBILITY;

	bool init_once(const AX_VDEC_MOD_ATTR_T* vdec_opt = nullptr);
	void deinit_once();
	bool is_finalized() const;

private:
	AXVdecLifecycle();
	~AXVdecLifecycle() noexcept EXPORT_VISIBILITY;
	std::atomic_bool inited_{false};
	std::mutex		 mutex_;
};
