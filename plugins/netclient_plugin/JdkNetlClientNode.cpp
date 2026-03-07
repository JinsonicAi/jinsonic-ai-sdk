#include "JdkNetlClientNode.hpp"

#include <unistd.h>

#include <chrono>
#include <ctime>

#include "jdk_control_meta.hpp"
#include "post_node_info.h"

namespace jdk_nodes {

// ---------------------------------------------------------------------------
// 构造 / 析构
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
// stop -- 停止 RTSP 连接和 schedule checker
// ---------------------------------------------------------------------------
void NetClientNode::stop() {
	gate_open();
	set_alive(false);
	// 唤醒可能正在 schedule 暂停中的主循环
	schedule_paused_.store(false);
	stop_schedule_checker();
	// 重要：不要在持 mutex_ 时调用 net_client->stop()（可能阻塞），否则与 handle_run 内部阻塞形成死锁。
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
// 按时布控 -- schedule checker 线程
// ---------------------------------------------------------------------------
void NetClientNode::start_schedule_checker() {
	// ===== 测试模式：test_cycle_seconds =====
	// 配置示例: { "test_cycle_seconds": 60 }
	// 效果: 运行 60 秒 → 暂停 60 秒 → 运行 60 秒 → …… 循环往复
	if (schedule_config_.contains("test_cycle_seconds")) {
		int cycle_sec = schedule_config_["test_cycle_seconds"].get<int>();
		if (cycle_sec < 5) cycle_sec = 5;  // 最低 5 秒，防止太快
		fmt::print("[Schedule] ★ TEST MODE: cycle = {} seconds (run {} → pause {} → repeat)\n",
				   cycle_sec, cycle_sec, cycle_sec);

		schedule_running_.store(true);
		schedule_joined_.store(false);  // 重置 join 标志（新线程启动）
		schedule_thread_ = std::thread([this, cycle_sec]() {
			fmt::print("[Schedule] Test checker thread started\n");
			while (schedule_running_.load()) {
				// ---- 运行阶段 ----
				fmt::print("[Schedule] ▶ TEST: running phase ({} sec)\n", cycle_sec);
				schedule_paused_.store(false);
				for (int i = 0; i < cycle_sec && schedule_running_.load(); ++i) {
					std::this_thread::sleep_for(std::chrono::seconds(1));
				}
				if (!schedule_running_.load()) break;

				// ---- 暂停阶段 ----
				fmt::print("[Schedule] ⏸ TEST: pause phase ({} sec)\n", cycle_sec);
				schedule_paused_.store(true);
				for (int i = 0; i < cycle_sec && schedule_running_.load(); ++i) {
					std::this_thread::sleep_for(std::chrono::seconds(1));
				}
			}
			schedule_paused_.store(false);
			fmt::print("[Schedule] Test checker thread exited\n");
		});
		return;  // 测试模式下不走正常逻辑
	}

	// ===== 正常模式 =====
	// 如果没有有效的 schedule 配置，则不启动检查器（7x24 运行）
	if (schedule_config_.empty() || !schedule_config_.contains("schedule")) {
		fmt::print("[Schedule] No schedule config, running 24/7\n");
		return;
	}
	auto& sched = schedule_config_["schedule"];
	if (!sched.is_array() || sched.empty()) {
		fmt::print("[Schedule] Empty schedule array, running 24/7\n");
		return;
	}

	// 如果所有天都被禁用（全部 enabled=false），等同于未配置，不启动检查器
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

	// 启动前先做一次初始检查
	bool initial_in_schedule = is_in_schedule_now();
	if (!initial_in_schedule) {
		fmt::print("[Schedule] Not in active period at startup, starting paused\n");
		schedule_paused_.store(true);
	}

	schedule_running_.store(true);
	schedule_joined_.store(false);  // 重置 join 标志（新线程启动）
	schedule_thread_ = std::thread([this]() {
		fmt::print("[Schedule] Checker thread started\n");
		while (schedule_running_.load()) {
			bool should_run		 = is_in_schedule_now();
			bool currently_paused = schedule_paused_.load();

			if (should_run && currently_paused) {
				fmt::print("[Schedule] ▶ Entering active period, resuming...\n");
				schedule_paused_.store(false);
			} else if (!should_run && !currently_paused) {
				fmt::print("[Schedule] ⏸ Leaving active period, pausing...\n");
				schedule_paused_.store(true);
			}

			// 每 30 秒检查一次，但分段 sleep 以便快速退出
			for (int i = 0; i < 30 && schedule_running_.load(); ++i) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		}
		fmt::print("[Schedule] Checker thread exited\n");
	});
}

void NetClientNode::stop_schedule_checker() {
	schedule_running_.store(false);
	// CAS 保证只有一个线程执行 join（stop() 和 handle_run 可能并发调用此函数）
	bool expected = false;
	if (schedule_joined_.compare_exchange_strong(expected, true)) {
		if (schedule_thread_.joinable()) {
			schedule_thread_.join();
		}
	}
}

// ---------------------------------------------------------------------------
// 按时布控 -- 判断当前时间是否在布控时段内
// ---------------------------------------------------------------------------
bool NetClientNode::is_in_schedule_now() const {
	if (schedule_config_.empty() || !schedule_config_.contains("schedule")) {
		return true;  // 无配置 → 不限制，始终运行
	}
	const auto& sched = schedule_config_["schedule"];
	if (!sched.is_array() || sched.empty()) {
		return true;  // 空数组 → 不限制
	}

	// 1. 获取当前 UTC 时间
	auto now	 = std::chrono::system_clock::now();
	auto now_t	 = std::chrono::system_clock::to_time_t(now);
	std::tm utc_tm{};
	gmtime_r(&now_t, &utc_tm);

	// 2. 应用时区偏移（默认 +08:00）
	int tz_offset_min = 8 * 60;  // 默认东八区
	if (schedule_config_.contains("timezone")) {
		std::string tz = schedule_config_["timezone"].get<std::string>();
		// 解析 "+08:00" / "-05:30" 格式
		if (tz.size() >= 5 && (tz[0] == '+' || tz[0] == '-')) {
			int sign = (tz[0] == '+') ? 1 : -1;
			int hours = 0, mins = 0;
			// 寻找冒号分隔
			auto colon_pos = tz.find(':');
			if (colon_pos != std::string::npos) {
				hours = std::stoi(tz.substr(1, colon_pos - 1));
				mins  = std::stoi(tz.substr(colon_pos + 1));
			} else {
				hours = std::stoi(tz.substr(1, 2));
				if (tz.size() >= 4) mins = std::stoi(tz.substr(3, 2));
			}
			tz_offset_min = sign * (hours * 60 + mins);
		}
	}

	// 3. 计算本地时间
	int total_min_utc = utc_tm.tm_hour * 60 + utc_tm.tm_min;
	int total_min_local = total_min_utc + tz_offset_min;

	// 处理跨日
	int day_offset = 0;
	if (total_min_local < 0) {
		total_min_local += 1440;
		day_offset = -1;
	} else if (total_min_local >= 1440) {
		total_min_local -= 1440;
		day_offset = 1;
	}

	// 4. 计算星期几 (1=周一 ... 7=周日)
	// tm_wday: 0=Sunday, 1=Monday ... 6=Saturday
	int utc_wday = utc_tm.tm_wday;
	// 转为 ISO 格式: 1=Monday ... 7=Sunday
	int iso_wday = (utc_wday == 0) ? 7 : utc_wday;
	// 应用日期偏移
	iso_wday += day_offset;
	if (iso_wday < 1) iso_wday += 7;
	if (iso_wday > 7) iso_wday -= 7;

	// 5. 在 schedule 中查找对应 weekday 的条目
	for (const auto& entry : sched) {
		if (!entry.contains("weekday")) continue;
		int weekday = entry["weekday"].get<int>();
		if (weekday != iso_wday) continue;

		// 找到对应天
		// 检查 enabled 字段
		if (entry.contains("enabled") && entry["enabled"].is_boolean() && !entry["enabled"].get<bool>()) {
			return false;  // 该天被禁用
		}

		// 检查 periods
		if (!entry.contains("periods") || !entry["periods"].is_array()) {
			return true;  // enabled 但没有 periods → 全天运行
		}
		const auto& periods = entry["periods"];
		if (periods.empty()) {
			return true;  // enabled 但 periods 为空 → 全天运行
		}

		// 遍历 periods，检查当前分钟是否在某个时段内
		for (const auto& p : periods) {
			int start_min = p.value("start_min", 0);
			int end_min	  = p.value("end_min", 1440);
			if (total_min_local >= start_min && total_min_local < end_min) {
				return true;  // 在布控时段内
			}
		}
		return false;  // 找到了对应天，但不在任何时段内
	}

	// 如果 schedule 中没有当天条目，视为该天未配置 → 全天运行（不限制）
	return true;
}

// ---------------------------------------------------------------------------
// RTSP info 上报
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
// handle_run -- 主处理循环（含按时布控暂停逻辑）
// ---------------------------------------------------------------------------
void NetClientNode::handle_run(std::stop_token stoken) {
	int						  fps = 0;
	std::chrono::milliseconds delta;
	int						  skip = 0;
	std::string				  file_name;
	bool					  pause_drain_sent = false;

	fmt::print(fg(fmt::color::blue),
			   "NetClientNode::handle_run:device_id_:{}, group_:{}, channel_id_:{}\n",
			   device_id_, group_, channel_id_);

	// 启动布控定时检查线程
	start_schedule_checker();

	{
		std::lock_guard<std::mutex> lk(mutex_);
		net_client = std::make_shared<NetClient>(device_id_, group_, channel_id_, info_, task_name_);
	}

	while (!stoken.stop_requested() && is_alive()) {
		gate_knock();
		// ---- 按时布控暂停检查 ----
		if (schedule_paused_.load()) {
			if (!pause_drain_sent) {
				pend_meta(std::make_shared<jdk_objects::jdk_control_meta>(
					jdk_objects::jdk_control_type::PIPELINE_DRAIN, channel_id_));
				pause_drain_sent = true;
			}
			// 断开 RTSP（如果还连着）
			if (rtsp_init_) {
				std::shared_ptr<NetClient> local;
				{
					std::lock_guard<std::mutex> lk(mutex_);
					local = net_client;
					net_client.reset();
				}
				if (local) local->stop();
				rtsp_init_ = false;
				fmt::print("[Schedule] ⏸ RTSP disconnected for schedule pause\n");
			}

			// 休眠等待恢复：用 1 秒粒度轮询，保证 stop 时快速退出。
			// 同时每 5 秒上报一次 PAUSED 状态（保持心跳，防止前端判定超时）
			int pausedTick = 0;
			while (schedule_paused_.load() && !stoken.stop_requested() && is_alive()) {
				if ((pausedTick % 5) == 0) {
				set_input_rtsp_info(task_id_.c_str(), rtsp_url_.c_str(),
									"PAUSED", "schedule", 0);
				}
				pausedTick++;
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			if (stoken.stop_requested() || !is_alive()) break;

			// 恢复：重建 NetClient 实例
			fmt::print("[Schedule] ▶ RTSP resuming from schedule pause\n");
			pause_drain_sent = false;
			{
				std::lock_guard<std::mutex> lk(mutex_);
				net_client = std::make_shared<NetClient>(device_id_, group_, channel_id_, info_, task_name_);
			}
			continue;  // 回到循环顶部，重新 lazy init RTSP
		}
		// ---- 以下为原有逻辑 ----
		auto last_time = std::chrono::system_clock::now();
		MetricsReporter::ScopedTimer timer(reporter_);

		// lazy init rtsp
		if (!rtsp_init_) {
			std::shared_ptr<NetClient> local;
			{
				std::lock_guard<std::mutex> lk(mutex_);
				local = net_client;
			}
			if (stoken.stop_requested() || !is_alive()) break;
			if (local) {
				// start 可能耗时/阻塞：放在锁外执行，避免 stop() 互锁
				rtsp_init_ = local->start(rtsp_url_);
			}
			// start 返回后立即检查 stop 信号
			if (stoken.stop_requested() || !is_alive()) break;
			// 可中断等待 1 秒（每 100ms 检查一次 stop 信号）
			for (int w = 0; w < 10 && !stoken.stop_requested() && is_alive(); ++w) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			continue;
		}

		fps	  = fps < 1 ? 25 : fps;
		delta = std::chrono::milliseconds(1000 / fps) * (skip_interval + 1);

		// need skip
		if (skip < skip_interval) {
			skip++;
			continue;
		}
		skip = 0;

		std::shared_ptr<AXVideoFrame> frame{nullptr};
		std::shared_ptr<NetClient> local;
		{
			std::lock_guard<std::mutex> lk(mutex_);
			local = net_client;
		}
		if (stoken.stop_requested() || !is_alive()) break;
		if (local) {
			// get_frame 可能阻塞：放在锁外执行，保证 stop() 能调用 local->stop() 来打断阻塞
			frame = local->get_frame((std::atomic<bool>*)alive_ptr());
		}
		// get_frame 阻塞返回后立即检查 stop 信号
		if (stoken.stop_requested() || !is_alive()) break;
		if (!frame) {
			usleep(30000);  // 与原逻辑一致，避免多次短 sleep 带来的调度/粒度开销
			continue;
		}

		this->frame_index++;
		if (auto meta = build_frame_meta(frame, frame_index, channel_id_, fps); meta) {
			assert(meta->result_map_.empty());
			out_queue_push(meta, (std::atomic<bool>*)alive_ptr());
		}

		// Minimal reporting: only transmit the changing fps/delay
		std::string resolution = std::to_string(frame->width()) + "x" + std::to_string(frame->height());
		std::string codec	   = (frame->enType == 96 ? "H264" : "H265");
		reporter_.report_input_rtsp(resolution, codec, 25);

		// fps 节奏控制：单次 sleep 保证定时精度，避免多段 10ms sleep 被系统舍入导致帧率折半
		auto snap = std::chrono::system_clock::now() - last_time;
		snap	  = std::chrono::duration_cast<std::chrono::milliseconds>(snap);
		if (snap < delta) {
			std::this_thread::sleep_for(delta - snap);
		}
	}

	fmt::print("[NetClientNode] handle_run exiting loop: {}\n", task_id_);
	stop_schedule_checker();
	this->out_queue_push(nullptr, (std::atomic<bool>*)alive_ptr());
	fmt::print("[NetClientNode] handle_run done: {}\n", task_id_);
}

// ---------------------------------------------------------------------------
// handle_control_meta -- 处理控制消息
// ---------------------------------------------------------------------------
std::shared_ptr<jdk_objects::jdk_meta> NetClientNode::handle_control_meta(
	std::shared_ptr<jdk_objects::jdk_control_meta> meta) {
	if (!meta) return nullptr;
	std::cout << "[NetClientNode] handle_control_meta: control_type = " << meta->control_type << std::endl;

	if (meta->control_type == jdk_objects::jdk_control_type::SPEAK) stop();
	return meta;
}

}  // namespace jdk_nodes
