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

// 插件生命周期状态
enum class PluginState {
	Installed,  // 已安装但未加载（如 disabled）
	Loaded,     // 已加载并注册到 NodeFactory
	Disabled,   // 用户主动禁用
	Error,      // 加载失败
};

// 待定操作（温和模式：等任务停止后自动执行）
struct PendingAction {
	enum class Type { None, Update, Disable };
	Type        type = Type::None;
	std::string package_path;    // 仅 Update 使用
	std::string scheduled_at;    // ISO 8601
	std::string reason;          // e.g. "in-use-by-tasks"
};

struct PluginInfo {
	std::string	   name;
	std::string	   path;
	nlohmann::json config;
	void*		   handle					= nullptr;
	void (*cleanup_func)(SDKInterface* sdk) = nullptr;

	// --- 生命周期管理字段 ---
	PluginState    state            = PluginState::Installed;
	bool           enabled          = true;
	std::string    version;
	std::string    previous_version;  // 回滚用
	uint64_t       load_time_ms     = 0;
	uint32_t       error_count      = 0;
	std::string    last_error;
	std::string    installed_at;      // ISO 8601
	std::string    updated_at;        // ISO 8601
	PendingAction  pending_action;
};

// 插件卸载清理结果
struct PluginRemoveResult {
	bool                     success = false;
	std::string              error;
	std::string              plugin_type;
	std::string              version;
	bool                     was_loaded = false;      // 是否热卸载了 .so
	std::vector<std::string> deleted_model_files;     // 待删除的模型文件路径
	uint64_t                 freed_bytes = 0;          // 回收的磁盘空间
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
	nlohmann::json inspect_package(const std::string& plugin_path, std::string* err = nullptr) const;

	const std::vector<PluginInfo>& get_loaded_plugins() const { return plugins_; }
	std::vector<PluginInfo> get_loaded_plugins_snapshot() const {
		std::lock_guard<std::mutex> lk(cwd_m_);
		return plugins_;
	}

	nlohmann::json get_component_structure() const {
		std::lock_guard<std::mutex> lk(cwd_m_);
		return component_structure_;
	}

	// --- 插件生命周期管理 ---

	// 卸载单个插件（cleanup + dlclose + 从 component_structure_ 移除）
	// 调用方需确保无任务引用该插件（安全约束）
	bool unloadSingle(const std::string& type, std::string* err = nullptr);

	// 重载单个插件（先 unload 再 load）
	bool reloadSingle(const std::string& type, std::string* err = nullptr);

	// 完整卸载：热卸载 .so（cleanup→dlclose→unregister）+ 从 plugins_ 移除 + 收集待删模型路径
	// 调用方需确保无任务引用该插件，且卸载后调用 save_plugin_state()
	// 返回的 deleted_model_files 由调用方异步删除以避免阻塞
	PluginRemoveResult removeSingle(const std::string& type);

	// 安装后注册：将新安装的插件注册到内存（state=Installed，不 dlopen）
	// 使 Phase1 立即包含该插件，无需 Phase2 磁盘扫描
	void registerInstalled(const std::string& type, const std::string& path,
	                       const nlohmann::json& config, const std::string& version);

	// 启用/禁用
	bool set_plugin_enabled(const std::string& type, bool enabled, std::string* err = nullptr);
	bool is_plugin_enabled(const std::string& type) const;

	// 查找插件
	PluginInfo* find_plugin_by_type(const std::string& type);
	const PluginInfo* find_plugin_by_type(const std::string& type) const;

	// 版本比较：返回 -1 (a<b), 0 (a==b), 1 (a>b)
	static int compare_versions(const std::string& a, const std::string& b);

	// 待定操作
	void set_pending_action(const std::string& type, const PendingAction& action);
	void clear_pending_action(const std::string& type);
	std::vector<std::pair<std::string, PendingAction>> get_all_pending_actions() const;

	// 状态持久化
	bool load_plugin_state(const std::string& state_path = "");
	bool save_plugin_state() const;

	// 获取插件根目录
	const std::string& get_plugin_root() const { return plugin_root_; }

private:
	void remove_component_by_type(const std::string& type);
	void add_component_from_config(const nlohmann::json& config);

	std::vector<PluginInfo> plugins_;
	SDKInterface*			sdk_ = nullptr;
	std::string             plugin_root_;
	std::string             state_file_path_;

	nlohmann::json component_structure_ = nlohmann::json::array();
	//

	std::unordered_map<std::string, std::string> cache_;
	mutable std::mutex							 cwd_m_;
	// io_uring									 ring_;
	ThreadPool	pool_;
	bool		support_local_ = false;
	std::string target_soc_{};

	// 启动时从 state file 预加载的 enabled/disabled 缓存（避免重复读磁盘）
	std::unordered_map<std::string, bool> state_enabled_cache_;
};
