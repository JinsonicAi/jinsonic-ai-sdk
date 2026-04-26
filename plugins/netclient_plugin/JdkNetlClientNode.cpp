#include "JdkNetlClientNode.hpp"

#include <unistd.h>

#include <chrono>
#include <ctime>

#include "jdk_control_meta.hpp"
#include "post_node_info.h"

namespace jdk_nodes {

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
NetClientNode::NetClientNode(std::string node_name, std::string rtsp_url, int device_id,
							 int group, int channel, stream_info info,
							 std::string task_id, std::string task_name,
							 nlohmann::json schedule_config)
	: rtsp_url_(rtsp_url),
	  device_id_(device_id),
	  group_(group),
	  channel_id_(channel),
	  info_(info),
	  task_id_(task_id),
	  task_name_(task_name),
	  schedule_config_(std::move(schedule_config)) {
	fmt::print("NetClientNode::NetClientNode:device_id_:{}, group_:{}, channel_id_:{}\n",
			   device_id_, group_, channel_id_);
	reporter_.set_input_rtsp_config({task_id_, PLUGIN_NODE_NAME, rtsp_url_});

	if (!schedule_config_.empty()) {
		fmt::print("[Schedule] config loaded: {}\n", schedule_config_.dump());
	}

	fmt::print("✅ NetClientNode constructed! BUILD TIME: {} {}\n", __DATE__, __TIME__);
}

NetClientNode::~NetClientNode() {
	stop_schedule_checker();
	gate_open();
	stop();
	fmt::print("✅ NetClientNode destructed!\n");
}

// ---------------------------------------------------------------------------
// stop -- Stop RTSP connection and schedule checker
// ---------------------------------------------------------------------------
void NetClientNode::stop() {
	gate_open();
	set_alive(false);
	// wake up main loop if it's in schedule pause
	schedule_paused_.store(false);
	stop_schedule_checker();
	// important: do not call net_client->stop() while holding mutex_ (may block), else deadlock
	std::shared_ptr<NetClient> local;
	{
		std::lock_guard<std::mutex> lk(mutex_);
		local = net_client;
		net_client.reset();
	}
	if (local) {
		local->stop();
	}
	fmt::print("✅ NetClientNode stop ok!\n");
}

// ---------------------------------------------------------------------------
// Time-based control -- schedule checker thread
// ---------------------------------------------------------------------------
void NetClientNode::start_schedule_checker() {
	// ===== Test mode: test_cycle_seconds =====
	// Config example: { "test_cycle_seconds": 60 }
	// Effect: run 60s -> pause 60s -> run 60s -> ... repeat
	if (schedule_config_.contains("test_cycle_seconds")) {
		int cycle_sec = schedule_config_["test_cycle_seconds"].get<int>();
		if (cycle_sec < 5)
			cycle_sec = 5;	// minimum 5 seconds to prevent too fast
		fmt::print("[Schedule] ★ TEST MODE: cycle = {} seconds (run {} → pause {} → repeat)\n",
				   cycle_sec, cycle_sec, cycle_sec);

		schedule_running_.store(true);
		schedule_joined_.store(false);	// reset join flag (new thread starting)
		schedule_thread_ = std::thread([this, cycle_sec]() {
			fmt::print("[Schedule] Test checker thread started\n");
			while (schedule_running_.load()) {
				// ---- running phase ----
				fmt::print("[Schedule] ▶ TEST: running phase ({} sec)\n", cycle_sec);
				schedule_paused_.store(false);
				for (int i = 0; i < cycle_sec && schedule_running_.load(); ++i) {
					std::this_thread::sleep_for(std::chrono::seconds(1));
				}
				if (!schedule_running_.load())
					break;

				// ---- pause phase ----
				fmt::print("[Schedule] ⏸ TEST: pause phase ({} sec)\n", cycle_sec);
				schedule_paused_.store(true);
				for (int i = 0; i < cycle_sec && schedule_running_.load(); ++i) {
					std::this_thread::sleep_for(std::chrono::seconds(1));
				}
			}
			schedule_paused_.store(false);
			fmt::print("[Schedule] Test checker thread exited\n");
		});
		return;	 // test mode: skip normal logic
	}

	// ===== normal mode =====
	// if no valid schedule config, don't start checker (run 24/7)
	if (schedule_config_.empty() || !schedule_config_.contains("schedule")) {
		fmt::print("[Schedule] No schedule config, running 24/7\n");
		return;
	}
	auto& sched = schedule_config_["schedule"];
	if (!sched.is_array() || sched.empty()) {
		fmt::print("[Schedule] Empty schedule array, running 24/7\n");
		return;
	}

	// if all days disabled (all enabled=false), equivalent to no config, don't start checker
	bool any_day_enabled = false;
	for (const auto& entry : sched) {
		if (!entry.contains("enabled") || entry["enabled"].get<bool>()) {
			any_day_enabled = true;
			break;
		}
	}
	if (!any_day_enabled) {
		fmt::print("[Schedule] All days disabled, treating as unconfigured, running 24/7\n");
		return;
	}

	// do initial check before starting
	bool initial_in_schedule = is_in_schedule_now();
	if (!initial_in_schedule) {
		fmt::print("[Schedule] Not in active period at startup, starting paused\n");
		schedule_paused_.store(true);
	}

	schedule_running_.store(true);
	schedule_joined_.store(false);	// reset join flag (new thread starting)
	schedule_thread_ = std::thread([this]() {
		fmt::print("[Schedule] Checker thread started\n");
		while (schedule_running_.load()) {
			bool should_run		  = is_in_schedule_now();
			bool currently_paused = schedule_paused_.load();

			if (should_run && currently_paused) {
				fmt::print("[Schedule] ▶ Entering active period, resuming...\n");
				schedule_paused_.store(false);
			} else if (!should_run && !currently_paused) {
				fmt::print("[Schedule] ⏸ Leaving active period, pausing...\n");
				schedule_paused_.store(true);
			}

			// check every 30s, but segmented sleep for quick exit
			for (int i = 0; i < 30 && schedule_running_.load(); ++i) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		}
		fmt::print("[Schedule] Checker thread exited\n");
	});
}

void NetClientNode::stop_schedule_checker() {
	schedule_running_.store(false);
	// CAS ensures only one thread executes join (stop() and handle_run may call concurrently)
	bool expected = false;
	if (schedule_joined_.compare_exchange_strong(expected, true)) {
		if (schedule_thread_.joinable()) {
			schedule_thread_.join();
		}
	}
}

// ---------------------------------------------------------------------------
// Time-based control -- check if current time is in scheduled period
// ---------------------------------------------------------------------------
bool NetClientNode::is_in_schedule_now() const {
	if (schedule_config_.empty() || !schedule_config_.contains("schedule")) {
		return true;  // no config -> no restriction, always run
	}
	const auto& sched = schedule_config_["schedule"];
	if (!sched.is_array() || sched.empty()) {
		return true;  // empty array -> no restriction
	}

	// 1. get current UTC time
	auto	now	  = std::chrono::system_clock::now();
	auto	now_t = std::chrono::system_clock::to_time_t(now);
	std::tm utc_tm{};
	gmtime_r(&now_t, &utc_tm);

	// 2. apply timezone offset (default +08:00)
	int tz_offset_min = 8 * 60;	 // default UTC+8
	if (schedule_config_.contains("timezone")) {
		std::string tz = schedule_config_["timezone"].get<std::string>();
		// parse "+08:00" / "-05:30" format
		if (tz.size() >= 5 && (tz[0] == '+' || tz[0] == '-')) {
			int sign  = (tz[0] == '+') ? 1 : -1;
			int hours = 0, mins = 0;
			// find colon separator
			auto colon_pos = tz.find(':');
			if (colon_pos != std::string::npos) {
				hours = std::stoi(tz.substr(1, colon_pos - 1));
				mins  = std::stoi(tz.substr(colon_pos + 1));
			} else {
				hours = std::stoi(tz.substr(1, 2));
				if (tz.size() >= 4)
					mins = std::stoi(tz.substr(3, 2));
			}
			tz_offset_min = sign * (hours * 60 + mins);
		}
	}

	// 3. calculate local time
	int total_min_utc	= utc_tm.tm_hour * 60 + utc_tm.tm_min;
	int total_min_local = total_min_utc + tz_offset_min;

	// handle day boundary crossing
	int day_offset = 0;
	if (total_min_local < 0) {
		total_min_local += 1440;
		day_offset = -1;
	} else if (total_min_local >= 1440) {
		total_min_local -= 1440;
		day_offset = 1;
	}

	// 4. calculate weekday (1=Monday ... 7=Sunday)
	// tm_wday: 0=Sunday, 1=Monday ... 6=Saturday
	int utc_wday = utc_tm.tm_wday;
	// convert to ISO format: 1=Monday ... 7=Sunday
	int iso_wday = (utc_wday == 0) ? 7 : utc_wday;
	// apply day offset
	iso_wday += day_offset;
	if (iso_wday < 1)
		iso_wday += 7;
	if (iso_wday > 7)
		iso_wday -= 7;

	// 5. find entry in schedule for corresponding weekday
	for (const auto& entry : sched) {
		if (!entry.contains("weekday"))
			continue;
		int weekday = entry["weekday"].get<int>();
		if (weekday != iso_wday)
			continue;

		// found entry for this day
		// check enabled field
		if (entry.contains("enabled") && entry["enabled"].is_boolean() && !entry["enabled"].get<bool>()) {
			return false;  // this day is disabled
		}

		// check periods
		if (!entry.contains("periods") || !entry["periods"].is_array()) {
			return true;  // enabled but no periods -> run all day
		}
		const auto& periods = entry["periods"];
		if (periods.empty()) {
			return true;  // enabled but periods empty -> run all day
		}

		// iterate periods, check if current minute is in any time slot
		for (const auto& p : periods) {
			int start_min = p.value("start_min", 0);
			int end_min	  = p.value("end_min", 1440);
			if (total_min_local >= start_min && total_min_local < end_min) {
				return true;  // in scheduled period
			}
		}
		return false;  // found day, but not in any time slot
	}

	// if no entry for today in schedule, treat as unconfigured -> run all day (no restriction)
	return true;
}

// ---------------------------------------------------------------------------
// Report RTSP info
// ---------------------------------------------------------------------------
void NetClientNode::report_rtsp_info(const std::shared_ptr<AXVideoFrame>& frame, int fps) {
	std::string resolution = std::to_string(frame->width()) + "x" + std::to_string(frame->height());
	const char* codec	   = (frame->enType == 96 ? "H264" : "H265");

	set_input_rtsp_info(
		task_id_.c_str(),
		rtsp_url_.c_str(),
		codec,
		resolution.c_str(),
		fps);
}

// ---------------------------------------------------------------------------
// build_frame_meta
// ---------------------------------------------------------------------------
std::shared_ptr<jdk_objects::jdk_frame_meta> NetClientNode::build_frame_meta(
	const std::shared_ptr<AXVideoFrame>& frame,
	int									 frame_index,
	int									 channel_id,
	int									 fps) {
	uint64_t timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
								std::chrono::system_clock::now().time_since_epoch())
								.count();

	return make_frame_meta(frame, frame_index, channel_id,
						   frame->width(), frame->height(), fps, timestamp_ms);
}

// ---------------------------------------------------------------------------
// handle_run -- main processing loop (with time-based control pause logic)
// ---------------------------------------------------------------------------
void NetClientNode::handle_run(std::stop_token stoken) {
	int			fps	 = 0;
	int			skip = 0;
	std::string file_name;
	bool		pause_drain_sent = false;
	// measured FPS stats: sliding window (last 30 frames)
	std::deque<std::chrono::steady_clock::time_point> frame_ts_window;
	constexpr int									  kFpsWindow = 30;
	fmt::print(fg(fmt::color::blue),
			   "NetClientNode::handle_run:device_id_:{}, group_:{}, channel_id_:{}\n",
			   device_id_, group_, channel_id_);

	// start time-based control checker thread
	start_schedule_checker();

	{
		std::lock_guard<std::mutex> lk(mutex_);
		net_client = std::make_shared<NetClient>(device_id_, group_, channel_id_, info_, task_name_);
	}

	while (!stoken.stop_requested() && is_alive()) {
		gate_knock();
		// ---- time-based control pause check ----
		if (schedule_paused_.load()) {
			if (!pause_drain_sent) {
				pend_meta(std::make_shared<jdk_objects::jdk_control_meta>(
					jdk_objects::jdk_control_type::PIPELINE_DRAIN, channel_id_));
				pause_drain_sent = true;
			}
			// disconnect RTSP (if still connected)
			if (rtsp_init_) {
				std::shared_ptr<NetClient> local;
				{
					std::lock_guard<std::mutex> lk(mutex_);
					local = net_client;
					net_client.reset();
				}
				if (local)
					local->stop();
				rtsp_init_ = false;
				fmt::print("[Schedule] ⏸ RTSP disconnected for schedule pause\n");
			}

			// sleep and wait for resume: poll with 1s granularity to ensure fast exit on stop
			// report PAUSED status every 5s to maintain heartbeat and prevent timeout
			int pausedTick = 0;
			while (schedule_paused_.load() && !stoken.stop_requested() && is_alive()) {
				if ((pausedTick % 5) == 0) {
					set_input_rtsp_info(task_id_.c_str(), rtsp_url_.c_str(),
										"PAUSED", "schedule", 0);
				}
				pausedTick++;
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			if (stoken.stop_requested() || !is_alive())
				break;

			// resume: rebuild NetClient instance
			fmt::print("[Schedule] ▶ RTSP resuming from schedule pause\n");
			pause_drain_sent = false;
			{
				std::lock_guard<std::mutex> lk(mutex_);
				net_client = std::make_shared<NetClient>(device_id_, group_, channel_id_, info_, task_name_);
			}
			continue;  // go back to loop top, re-initialize RTSP lazily
		}
		// ---- original logic follows ----
		MetricsReporter::ScopedTimer timer(reporter_);

		// lazy initialize RTSP
		if (!rtsp_init_) {
			std::shared_ptr<NetClient> local;
			{
				std::lock_guard<std::mutex> lk(mutex_);
				local = net_client;
			}
			if (stoken.stop_requested() || !is_alive())
				break;
			if (local) {
				// start may take time/block: execute outside lock to avoid stop() deadlock
				rtsp_init_ = local->start(rtsp_url_);
			}
			// check stop signal immediately after start returns
			if (stoken.stop_requested() || !is_alive())
				break;
			// interruptible wait 1s (check stop signal every 100ms)
			for (int w = 0; w < 10 && !stoken.stop_requested() && is_alive(); ++w) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			continue;
		}

		// need skip
		if (skip < skip_interval) {
			skip++;
			continue;
		}
		skip = 0;

		std::shared_ptr<AXVideoFrame> frame{nullptr};
		std::shared_ptr<NetClient>	  local;
		{
			std::lock_guard<std::mutex> lk(mutex_);
			local = net_client;
		}
		if (stoken.stop_requested() || !is_alive())
			break;
		if (local) {
			// get_frame 可能阻塞：放在锁外执行，保证 stop() 能调用 local->stop() 来打断阻塞
			frame = local->get_frame((std::atomic<bool>*)alive_ptr());
		}
		// check stop signal immediately after get_frame returns
		if (stoken.stop_requested() || !is_alive())
			break;
		if (!frame) {
			usleep(30000);	// consistent with original logic, avoid scheduling overhead from multiple short sleeps
			continue;
		}

		this->frame_index++;

		// measured FPS: sliding window stats
		auto now_tp = std::chrono::steady_clock::now();
		frame_ts_window.push_back(now_tp);
		if ((int)frame_ts_window.size() > kFpsWindow)
			frame_ts_window.pop_front();
		if ((int)frame_ts_window.size() >= 2) {
			auto span_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
							   frame_ts_window.back() - frame_ts_window.front())
							   .count();
			if (span_ms > 0)
				fps = static_cast<int>((frame_ts_window.size() - 1) * 1000.0 / span_ms + 0.5);
		}
		if (fps < 1)
			fps = 1;

		if (auto meta = build_frame_meta(frame, frame_index, channel_id_, fps); meta) {
			assert(meta->result_map_.empty());
			out_queue_push(meta, (std::atomic<bool>*)alive_ptr());
		}

		// Minimal reporting: only transmit the changing fps/delay
		std::string resolution = std::to_string(frame->width()) + "x" + std::to_string(frame->height());
		std::string codec	   = (frame->enType == 96 ? "H264" : "H265");
		reporter_.report_input_rtsp(resolution, codec, fps);

		// do not add extra sleep: get_frame itself blocks waiting for queue, naturally following real stream fps
		// extra sleep would stack on top of get_frame wait time, artificially lowering fps
	}

	fmt::print("[NetClientNode] handle_run exiting loop: {}\n", task_id_);
	stop_schedule_checker();
	this->out_queue_push(nullptr, (std::atomic<bool>*)alive_ptr());
	fmt::print("[NetClientNode] handle_run done: {}\n", task_id_);
}

// ---------------------------------------------------------------------------
// handle_control_meta -- handle control messages
// ---------------------------------------------------------------------------
std::shared_ptr<jdk_objects::jdk_meta> NetClientNode::handle_control_meta(
	std::shared_ptr<jdk_objects::jdk_control_meta> meta) {
	if (!meta)
		return nullptr;
	std::cout << "[NetClientNode] handle_control_meta: control_type = " << meta->control_type << std::endl;

	if (meta->control_type == jdk_objects::jdk_control_type::SPEAK)
		stop();
	return meta;
}

}  // namespace jdk_nodes
