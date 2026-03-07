#pragma once
// rmwei add .
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#include "post_node_info.h"

class MetricsReporter {
public:
	using ms  = std::chrono::milliseconds;
	using i64 = long long;
	// interval_sec: throttling cycle (seconds), default 5 seconds
	explicit MetricsReporter(int interval_sec = 5)
		: interval_(interval_sec),
		  last_report_time_(std::chrono::steady_clock::now() - std::chrono::seconds(interval_sec)) {}

	// Automatically writes the elapsed time (ms) back to reporter_'s last_exec_time_ms_ when destructed.
	class ScopedTimer {
	public:
		explicit ScopedTimer(MetricsReporter& reporter, long long* out_ms = nullptr)
			: reporter_(reporter), out_(out_ms), start_(std::chrono::steady_clock::now()) {}
		~ScopedTimer() {
			auto end   = std::chrono::steady_clock::now();
			auto dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_).count();
			if (out_) *out_ = dt_ms;				 // Optional: write back to the caller variable
			reporter_.set_last_exec_time_ms(dt_ms);	 // Stored in reporter, used for algorithm reporting.
		}

	private:
		MetricsReporter&					  reporter_;
		long long*							  out_;
		std::chrono::steady_clock::time_point start_;
	};

	struct InputRtspConfig {
		std::string task_id;
		std::string plugin_id;
		std::string url;
	};

	struct AlgorithmConfig {
		std::string task_id;
		std::string plugin_id;
		std::string threshold;	// It is recommended to convert to a string during initialization.
	};

	struct OsdConfig {
		std::string task_id;
		std::string plugin_id;
	};

	struct OutputRtspConfig {
		std::string task_id;
		std::string plugin_id;
		std::string url;
		std::string codec;
		// std::string resolution;
	};

	void set_input_rtsp_config(InputRtspConfig cfg) {
		std::lock_guard<std::mutex> lk(mu_);
		in_cfg_ = std::move(cfg);
	}
	void set_algorithm_config(AlgorithmConfig cfg) {
		std::lock_guard<std::mutex> lk(mu_);
		alg_cfg_ = std::move(cfg);
	}
	void set_osd_config(OsdConfig cfg) {
		std::lock_guard<std::mutex> lk(mu_);
		osd_cfg_ = std::move(cfg);
	}
	void set_output_rtsp_config(OutputRtspConfig cfg) {
		std::lock_guard<std::mutex> lk(mu_);
		out_cfg_ = std::move(cfg);
	}

	void report_input_rtsp(std::string resolution, std::string codec, uint32_t fps, bool force = false) {
		if (!shouldReport(force)) return;
		std::lock_guard<std::mutex> lk(mu_);
		if (!in_cfg_) return;
		set_input_rtsp_info(in_cfg_->task_id.c_str(),
							in_cfg_->url.c_str(),
							codec.c_str(),
							resolution.c_str(),
							fps);
		stamp();
	}

	void report_algorithm(uint32_t fps, std::chrono::system_clock::time_point create_time, bool force = false) {
		if (!shouldReport(force)) return;
		std::lock_guard<std::mutex> lk(mu_);
		if (!alg_cfg_) return;
		auto delayed_time				   = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - create_time).count();
		delayed_time					   = std::max<i64>(0LL, delayed_time - last_exec_time_ms_);
		const std::string delayed_time_str = std::to_string(delayed_time) + " ms";
		const std::string time_str		   = std::to_string(last_exec_time_ms_) + " ms";
		set_algorithm_info(alg_cfg_->task_id.c_str(),
						   alg_cfg_->plugin_id.c_str(),
						   alg_cfg_->threshold.c_str(),
						   time_str.c_str(),
						   delayed_time_str.c_str(),
						   fps);
		stamp();
	}

	void report_osd(uint32_t fps, std::chrono::system_clock::time_point create_time, bool force = false) {
		if (!shouldReport(force)) return;
		std::lock_guard<std::mutex> lk(mu_);
		if (!osd_cfg_) return;
		auto delayed_time				   = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - create_time).count();
		delayed_time					   = std::max<i64>(0LL, delayed_time - last_exec_time_ms_);
		const std::string delayed_time_str = std::to_string(delayed_time) + " ms";
		const std::string time_str		   = std::to_string(last_exec_time_ms_) + " ms";
		set_osd_info(osd_cfg_->task_id.c_str(),
					 time_str.c_str(),
					 delayed_time_str.c_str(),
					 fps);
		stamp();
	}

	void report_output_rtsp(std::string resolution, uint32_t fps, std::chrono::system_clock::time_point create_time, bool force = false) {
		if (!shouldReport(force)) return;
		std::lock_guard<std::mutex> lk(mu_);
		if (!out_cfg_) return;
		auto delayed_time				   = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - create_time).count();
		delayed_time					   = std::max<i64>(0LL, delayed_time - last_exec_time_ms_);
		const std::string delayed_time_str = std::to_string(delayed_time) + " ms";
		const std::string time_str		   = std::to_string(last_exec_time_ms_) + " ms";

		set_output_rtsp_info(out_cfg_->task_id.c_str(),
							 out_cfg_->url.c_str(),
							 out_cfg_->codec.c_str(),
							 resolution.c_str(),
							 delayed_time_str.c_str(),
							 fps);
		stamp();
	}

	// Dynamically adjust the throttling cycle
	void set_interval(int interval_sec) {
		std::lock_guard<std::mutex> lk(mu_);
		interval_ = interval_sec;
	}

	// Manually set the last duration (can also be manually injected if ScopedTimer is not used).
	void set_last_exec_time_ms(long long ms) {
		std::lock_guard<std::mutex> lk(mu_);
		last_exec_time_ms_ = ms;
	}

private:
	bool shouldReport(bool force) const {
		if (force) return true;
		auto now = std::chrono::steady_clock::now();
		return std::chrono::duration_cast<std::chrono::seconds>(now - last_report_time_).count() >= interval_;
	}

	void stamp() {
		last_report_time_ = std::chrono::steady_clock::now();
	}

private:
	mutable std::mutex					  mu_;
	int									  interval_{5};
	std::chrono::steady_clock::time_point last_report_time_;

	// Four types of configurations (set one or more as needed)
	std::optional<InputRtspConfig>	in_cfg_;
	std::optional<AlgorithmConfig>	alg_cfg_;
	std::optional<OsdConfig>		osd_cfg_;
	std::optional<OutputRtspConfig> out_cfg_;

	// The last recorded duration (usually written by ScopedTimer for use in algorithm reporting)
	long long last_exec_time_ms_{0};
};
