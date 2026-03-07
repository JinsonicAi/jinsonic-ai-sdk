#include "JdkAlarmNode.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "DeviceInfo.hpp"
#include "SnapshotCleaner.hpp"
#include "anyx.hpp"
#include "post_node_info.h"

namespace jdk_nodes {
auto alarm_last_report_time = std::chrono::steady_clock::now();
using namespace std::chrono;

namespace {
int parse_env_int_clamped(const char* key, int defv, int min_v, int max_v) {
	const char* raw = std::getenv(key);
	if (!raw || !raw[0]) return defv;
	try {
		int v = std::stoi(raw);
		if (v < min_v) v = min_v;
		if (v > max_v) v = max_v;
		return v;
	} catch (...) {
		return defv;
	}
}

int snapshot_retention_days() {
	static const int kDays = parse_env_int_clamped("ALARM_SNAPSHOT_RETENTION_DAYS", 7, 1, 365);
	return kDays;
}

int64_t snapshot_retention_max_bytes() {
	static const int kMaxMB = parse_env_int_clamped("ALARM_SNAPSHOT_MAX_BYTES_MB", 500, 50, 102400);
	return static_cast<int64_t>(kMaxMB) * 1024 * 1024;
}
}  // namespace

alarmNode::alarmNode(std::string node_name, int device_id, std::string server_url, 
						 int interval_sec, std::string task_id, std::string task_name,
						 TTSConfig tts_config)
	: device_id_(device_id), http_server_url_(server_url), task_id_(task_id), 
	  task_name_(task_name), tts_config_(tts_config) {
	if (Capture_ == nullptr) {
		Capture_ = std::make_shared<HwCapture>(device_id_);
	}
	if (!http_server_url_.empty()) {
		ServerPusher_ = std::make_shared<AlarmPusher>(http_server_url_);
		reportDeviceInfo();
	}
	// 初始化TTS音柱播报器
	if (tts_config_.enabled && !tts_config_.speaker_ip.empty()) {
		TTSSpeaker_ = std::make_shared<TTSSpeaker>(tts_config_.speaker_ip);
		TTSSpeaker_->setVolume(tts_config_.volume);
		fmt::print("🔊 TTS Speaker enabled: {} (volume: {}, vcn: {})\n", 
				   tts_config_.speaker_ip, tts_config_.volume, tts_config_.vcn);
	}
	reporter_.set_algorithm_config({task_id_,
									PLUGIN_NODE_NAME,
									"-"});
	fmt::print("task_name: {}\n", task_name_.c_str());
	fmt::print("[Alarm] snapshot retention: days={}, max_bytes_mb={}\n",
			   snapshot_retention_days(),
			   snapshot_retention_max_bytes() / (1024 * 1024));
	fmt::print("✅ alarmNode constructed! BUILD TIME: {} {}\n", __DATE__, __TIME__);
}

alarmNode::~alarmNode() {
	stop();
	fmt::print("✅ alarmNode destructed!\n");
}

void alarmNode::stop() {
	set_alive(false);
	std::lock_guard<std::mutex> lk(mutex_);
	if (TTSSpeaker_) {
		TTSSpeaker_->cancel();  // 取消正在播放的TTS
		TTSSpeaker_.reset();
	}
	if (ServerPusher_) {
		ServerPusher_.reset();
	}
	if (Capture_) {
		Capture_.reset();
	}
	fmt::print("✅ alarmNode stop ok!\n");
}

void alarmNode::setTTSConfig(const TTSConfig& config) {
	std::lock_guard<std::mutex> lk(mutex_);
	tts_config_ = config;
	
	if (tts_config_.enabled && !tts_config_.speaker_ip.empty()) {
		if (!TTSSpeaker_ || TTSSpeaker_->getSpeakerIp() != tts_config_.speaker_ip) {
			TTSSpeaker_ = std::make_shared<TTSSpeaker>(tts_config_.speaker_ip);
		}
		TTSSpeaker_->setVolume(tts_config_.volume);
		fmt::print("🔊 TTS config updated: {} (volume: {})\n", 
				   tts_config_.speaker_ip, tts_config_.volume);
	} else {
		TTSSpeaker_.reset();
		fmt::print("🔇 TTS disabled\n");
	}
}

void alarmNode::speakTTS(const std::string& text) {
	if (!TTSSpeaker_ || text.empty()) return;
	
	TTSSpeaker::SpeakOptions options;
	options.vcn    = tts_config_.vcn;
	options.speed  = tts_config_.speed;
	options.volume = tts_config_.volume;
	options.queue  = true;  // 队列模式，避免打断
	
	TTSSpeaker_->speakAsync(text, options, tts_config_.debounce_ms);
}

void alarmNode::processTTSAlarm(const nlohmann::json& alarm_result) {
	if (!TTSSpeaker_ || !tts_config_.enabled) return;
	
	// 检查是否应该播报
	if (!AlarmTTS::shouldTTSSpeak(alarm_result)) return;
	
	// 提取TTS信息 (支持队列模式，多条依次播放)
	AlarmTTS::TTSInfo tts_info = AlarmTTS::extractTTSInfo(alarm_result);
	if (!tts_info.enabled || !tts_info.hasContent()) return;
	
	TTSSpeaker::SpeakOptions options;
	options.vcn    = tts_config_.vcn;
	options.speed  = tts_config_.speed;
	options.volume = tts_config_.volume;
	options.queue  = true;  // 队列模式，多条依次播放
	
	// 遍历播报队列，依次发送（音柱会自动排队播放）
	for (const auto& item : tts_info.items) {
		if (item.isUrlMode()) {
			// 播放音频URL (算法插件指定的预录音频)
			TTSSpeaker_->playUrlAsync(item.url, options, tts_config_.debounce_ms);
		} else {
			// TTS文字转语音
			TTSSpeaker_->speakAsync(item.text, options, tts_config_.debounce_ms);
		}
	}
}

inline int64_t get_current_timestamp_ms() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(
			   std::chrono::steady_clock::now().time_since_epoch())
		.count();
}

std::string get_current_time_string() {
	auto now		= system_clock::now();
	auto time_t_now = system_clock::to_time_t(now);

	return fmt::format("{:%Y-%m-%d %H:%M:%S}", fmt::localtime(time_t_now));
}

std::string generate_snapshot_filename(std::string_view prefix = "alarm", std::string_view ext = ".jpg") {
	// get the current time
	auto now		= std::chrono::system_clock::now();
	auto time_t_now = std::chrono::system_clock::to_time_t(now);
	auto ms			= std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
	std::error_code ec;
	std::filesystem::create_directories("./snapshot", ec);

	return fmt::format("./snapshot/{}_{}_{:03}{}",
					   prefix,
					   fmt::format("{:%Y%m%d_%H%M%S}", fmt::localtime(time_t_now)),
					   ms.count(),
					   ext);
}

void alarmNode::reportDeviceInfo() {
	auto& dev = DeviceInfo::instance();
	fmt::print("{:<7}: {}\n", "fwVer", dev.supportLocal() ? dev.fwVersion() : dev.deviceId());
	fmt::print("{:<7}: {}\n", "swVer", dev.swVersion());
	fmt::print("{:<7}: {}\n", "osVer", dev.OSVersion());
	fmt::print("{:<7}: {}\n", "hwVer", dev.swVersion());
	fmt::print("{:<7}: SC-{}\n", "devName", dev.targetSoc());
	nlohmann::json deviceInfo = {
		{"fwVer", dev.supportLocal() ? dev.fwVersion() : dev.deviceId()},
		{"swVer", dev.swVersion()},
		{"osVer", dev.OSVersion()},
		{"hwVer", dev.swVersion()},
		{"devName", "SC-" + dev.targetSoc()}};
	if (ServerPusher_) {
		ServerPusher_->sendAsync("/api/v1/device/report/info", deviceInfo.dump());
	}
}

void alarmNode::sendPushMessages(nlohmann::json& alarm_result) {
	const auto server_uri = alarm_result.value("server_push_uri", "");
	const auto alarm_uri  = alarm_result.value("alarm_push_uri", "");

	if (server_uri.empty() && alarm_uri.empty()) return;

	if (!alarm_result.contains("alarms") || !alarm_result["alarms"].is_array()) {
		throw std::runtime_error("Invalid alarms format");
	}

	for (auto& alarm : alarm_result["alarms"]) {
		const auto process_msg = [&](const std::string& type, const std::string& uri) {
			if (alarm.contains(type)) {
				const auto& msg = alarm[type];
				if (msg.contains("data") && !msg["data"].empty()) {
					ServerPusher_->sendAsync(uri, msg.dump());
				}
				alarm.erase(type);
			}
		};

		process_msg("server_push_msg", server_uri);
		process_msg("alarm_push_msg", alarm_uri);
	}

	cleanupMessages(alarm_result);
}

void alarmNode::cleanupMessages(nlohmann::json& alarm_result) {
	alarm_result.erase("server_push_uri");
	alarm_result.erase("alarm_push_uri");

	if (alarm_result.contains("alarms")) {
		for (auto& alarm : alarm_result["alarms"]) {
			alarm.erase("server_push_msg");
			alarm.erase("alarm_push_msg");
		}
	}
}

// handle frame meta one by one
std::shared_ptr<jdk_objects::jdk_meta> alarmNode::handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) {
	std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>> frame_meta_with_batch{meta};
	MetricsReporter::ScopedTimer							  timer(reporter_);
	std::lock_guard<std::mutex>								  lk(mutex_);
	if (!is_alive()) return nullptr;
	const std::string& msg_type = "PUSH_DEVICE_WARN_MESSAGE";
	int				   code		= 200;
	auto&			   canvas	= meta->frame_;
	if (!canvas) {	// extra safety‑guard
		fprintf(stderr, "❌ alarmNode received a nullptr frame, skipping\n");
		return jdk_node_base::handle_frame_meta(meta);
	}

	bool should_upload = false;

	jdk_objects::result_map_t local_copy;
	{
		std::lock_guard<std::mutex> lk(*meta->mtx_);
		local_copy = meta->result_map_;
	}

	for (const auto& [key, entry_sp] : local_copy) {
		const auto entry = entry_sp.load();
		if (!entry->alarm_fn) {
			std::cerr << "❗ alarm_fn is null for key: " << key << "\n";
			continue;
		}
		if (!entry->push_enabled) continue;
		// unify result access via anyx::view
		auto v = anyx::view(entry->result);
		if (!v) {
			std::cerr << "❗ result not set for key: " << key << "\n";
			continue;
		}
		int64_t now_ms	   = get_current_timestamp_ms();
		auto&	push_state = push_states_[key];
		int64_t elapsed	   = now_ms - push_state.last_push_time_ms;

		if (elapsed >= entry->push_interval_ms) {
			try {
				auto [alarm_result, snaps] = entry->alarm_fn(*v.a);
				if (alarm_result.is_null()) {
					std::cerr << "⚠️ alarm_fn returned null for: " << key << "\n";
					continue;
				}
				if (alarm_result.value("alarm_count", 0) <= 0) continue;
				should_upload	  = true;
				auto processFrame = [&](AX_VIDEO_FRAME_T* rawFrame) {
					if (!rawFrame) return;
					// try capturing a jpeg
						if (auto jpegFrame = Capture_->capture(rawFrame); jpegFrame) {
							// 1) asynchronous cleanup of old snapshot directories can be moved out of the loop and called only once as needed
							SnapshotCleaner::get().clean_async(
								"./snapshot",
								snapshot_retention_days(),
								snapshot_retention_max_bytes(),
								[](const std::string& deleted_path) {
									const int affected = delete_device_message_by_image_path(deleted_path.c_str(), 0);
									if (affected > 0) {
										fmt::print("[Alarm] snapshot cleanup removed db rows: {} path={}\n", affected, deleted_path);
									}
								});
							std::string snapshot_path = generate_snapshot_filename();
						jpegFrame->save_data(snapshot_path.c_str());
						std::string alarm_type = alarm_result.value("alarm_type", "Unknown");
						alarm_result["id"]	   = task_id_;
						alarm_result["name"]   = task_name_;
						alarm_result["img"]	   = snapshot_path;
						if (isLogin()) {
							SdkSendMessage(msg_type.c_str(),
										   "success",
										   code,
										   alarm_result.dump().c_str());
						}
						insert_device_message_item(task_id_.data(),
												   task_name_.data(),
												   alarm_type.c_str(),
												   alarm_result.dump().c_str(),
												   snapshot_path.c_str(),
												   get_current_time_string().c_str());
						push_alarm_to_server(snapshot_path,
											 task_id_,
											 task_name_,
											 alarm_type,
											 alarm_type);

						push_state.last_push_time_ms = now_ms;
					}
				};

				// send msg to server
				if (ServerPusher_) {
					sendPushMessages(alarm_result);
				} else {
					cleanupMessages(alarm_result);
				}
				
				// TTS音柱播报
				processTTSAlarm(alarm_result);

				if (should_upload) {
					if (snaps.empty()) {
						if (canvas) {
							processFrame(canvas->raw());
						}
					} else {
						for (auto& get_img : snaps) {
							if (get_img) {
								std::shared_ptr<AXVideoFrame> snapshotFrame = get_img();
								processFrame(snapshotFrame->raw());
							}
						}
					}
				}
			} catch (const std::exception& e) {
				std::cerr << "Alarm: Error processing [" << key << "]: " << e.what() << "\n";
			}
		}
	}
	// report node info
	reporter_.report_algorithm(jdk_node_base::node_fps(), meta->create_time);
	return jdk_node_base::handle_frame_meta(meta);
}

void alarmNode::handle_frame_meta(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& meta_with_batch) {
	const auto& frame_meta_with_batch = meta_with_batch;
	// run_infer_combinations(frame_meta_with_batch);
}

std::shared_ptr<jdk_objects::jdk_meta> alarmNode::handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) {
	if (!meta) return nullptr;
	std::cout << "[alarmNode] handle_control_meta: control_type = " << meta->control_type << std::endl;
	if (meta->control_type == jdk_objects::jdk_control_type::SPEAK) this->stop();

	return meta;
};

}  // namespace jdk_nodes
