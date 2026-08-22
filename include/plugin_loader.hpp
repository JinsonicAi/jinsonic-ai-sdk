#pragma once
#include <dlfcn.h>
#include <liburing.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <json.hpp>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "sdk_interface.hpp"

const std::vector<int>& ax_device_ids();

// plugin lifecycle states
enum class PluginState {
	Installed,	// installed but not loaded (e.g. disabled)
	Loaded,		// loaded and registered to NodeFactory
	Disabled,	// disabled by user
	Error,		// load failed
};

// Whole-directory discovery state. "Ready" means the initial scan has settled;
// individual plugins can still be Disabled or Error and are reported separately.
enum class PluginLoadState : uint8_t {
	NotStarted,
	Loading,
	Ready,
	Failed,
};

// pending actions (gentle mode: execute after tasks stop)
struct PendingAction {
	enum class Type { None,
					  Update,
					  Disable,
					  Uninstall };
	Type		type = Type::None;
	std::string package_path;  // used by Update only
	std::string scheduled_at;  // ISO 8601
	std::string reason;		   // e.g. "in-use-by-tasks"
};

struct PluginInfo {
	std::string	   name;
	std::string	   path;
	std::string	   runtime_so_path;  // disk-backed extracted image kept until dlclose
	nlohmann::json config;
	void*		   handle					= nullptr;
	void (*cleanup_func)(SDKInterface* sdk) = nullptr;

	// --- Lifecycle management fields ---
	PluginState	  state	  = PluginState::Installed;
	bool		  enabled = true;
	std::string	  version;
	std::string	  previous_version;	 // for rollback
	uint64_t	  load_time_ms = 0;
	uint32_t	  error_count  = 0;
	std::string	  last_error;
	std::string	  installed_at;	 // ISO 8601
	std::string	  updated_at;	 // ISO 8601
	PendingAction pending_action;
};

// plugin removal cleanup result
struct PluginRemoveResult {
	bool					 success = false;
	std::string				 error;
	std::string				 plugin_type;
	std::string				 version;
	bool					 was_loaded = false;   // whether .so was hot-unloaded
	std::vector<std::string> deleted_model_files;  // model file paths pending deletion
	uint64_t				 freed_bytes = 0;	   // freed disk space
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
						if (stop && tasks.empty())
							return;
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
		if (!has_shutdown_.compare_exchange_strong(expected, true))
			return;

		{
			std::lock_guard<std::mutex> lk(mtx);
			stop = true;
		}
		cv.notify_all();
		for (auto& w : workers) {
			if (w.joinable())
				w.join();
		}
	}
	~ThreadPool() {
		shutdown_and_wait();
	}
	template <class F>
	void enqueue(F&& f) {
		{
			std::lock_guard<std::mutex> lock(mtx);
			if (stop)
				throw std::runtime_error("enqueue on stopped ThreadPool");
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
	// threads=0 selects a memory-aware default; AIBOX_PLUGIN_LOAD_THREADS can
	// override it for measured deployments.
	PluginLoader(size_t threads = 0, unsigned queue_depth = 256);
	~PluginLoader();  // automatic cleanup of plugins

	bool		   load_all_plugins(const std::string& plugin_root, SDKInterface* sdk);
	void		   loadSingle(const std::string pluginPath);
	nlohmann::json inspect_package(const std::string& plugin_path, std::string* err = nullptr) const;

	const std::vector<PluginInfo>& get_loaded_plugins() const;
	std::vector<PluginInfo>       get_loaded_plugins_snapshot() const;
	// Inject the loaded package's authoritative, content-verified resource-file
	// arrays into a task before the plugin node is constructed.
	size_t                        bind_task_resource_files(const std::string& type,
											 nlohmann::json& task_config) const;
	nlohmann::json                get_component_structure() const;
	PluginLoadState               get_load_state() const noexcept;
	bool                          is_initial_load_ready() const noexcept;
	size_t                        get_discovered_plugin_count() const noexcept;
	size_t                        get_settled_plugin_count() const noexcept;
	uint64_t                      get_component_revision() const noexcept;
	static size_t                 core_object_size() noexcept;

	// --- plugin lifecycle management ---
	bool unloadSingle(const std::string& type, std::string* err = nullptr);

	// Reload a single plugin (unload then load)
	bool reloadSingle(const std::string& type, std::string* err = nullptr);

	// Returned deleted_model_files should be deleted asynchronously by caller to avoid blocking
	PluginRemoveResult removeSingle(const std::string& type);

	// Allows Phase1 to include the plugin immediately without Phase2 disk scan
	void registerInstalled(const std::string& type, const std::string& path,
						   const nlohmann::json& config, const std::string& version);

	// enable/disable
	bool set_plugin_enabled(const std::string& type, bool enabled, std::string* err = nullptr);
	bool is_plugin_enabled(const std::string& type) const;

	// find plugin
	PluginInfo*		  find_plugin_by_type(const std::string& type);
	const PluginInfo* find_plugin_by_type(const std::string& type) const;

	// Invoke an explicitly exported C ABI function from one loaded plugin.
	// The loader lock stays held for the whole call so a concurrent hot-unload
	// cannot dlclose the DSO between dlsym() and the function invocation.
	bool with_plugin_symbol(const std::string& type,
							const char* symbol,
							const std::function<bool(void*)>& invoke,
							std::string* err = nullptr) const;

	// version compare: returns -1 (a<b), 0 (a==b), 1 (a>b)
	static int compare_versions(const std::string& a, const std::string& b);

	// pending actions
	void											   set_pending_action(const std::string& type, const PendingAction& action);
	void											   clear_pending_action(const std::string& type);
	std::vector<std::pair<std::string, PendingAction>> get_all_pending_actions() const;

	// state persistence
	bool load_plugin_state(const std::string& state_path = "");
	bool save_plugin_state() const;

	// get plugin root directory
	const std::string& get_plugin_root() const;

private:
	void remove_component_by_type(const std::string& type);
	void add_component_from_config(const nlohmann::json& config);

	std::vector<PluginInfo> plugins_;
	SDKInterface*			sdk_ = nullptr;
	std::string				plugin_root_;
	std::string				state_file_path_;

	nlohmann::json component_structure_ = nlohmann::json::array();
	//

	std::unordered_map<std::string, std::string> cache_;
	mutable std::mutex							 cwd_m_;
	mutable std::mutex							 component_m_;
	// io_uring									 ring_;
	size_t		load_threads_ = 1;
	ThreadPool	pool_;
	bool		support_local_ = false;
	std::string target_soc_{};
	std::atomic<PluginLoadState> load_state_{PluginLoadState::NotStarted};
	std::atomic<size_t> discovered_plugin_count_{0};
	std::atomic<size_t> settled_plugin_count_{0};
	std::atomic<uint64_t> component_revision_{0};

	// enabled/disabled cache preloaded from state file at startup (avoid duplicate disk reads)
	std::unordered_map<std::string, bool> state_enabled_cache_;
};
