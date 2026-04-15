#include "JdkFireDetectNode.hpp"

#include <iomanip>
#include <optional>

#include "DevProtoDef.hpp"
#include "DeviceInfo.hpp"
#include "HwIvps.hpp"
#include "alg_comm.hpp"
#include "anyx.hpp"
#include "ax_ivps_api.h"
#include "base64.hpp"
#include "post_node_info.h"

namespace jdk_nodes {

std::string get_current_iso_time() {
	using namespace std::chrono;

	auto now	   = system_clock::now();
	auto in_time_t = system_clock::to_time_t(now);
	auto ms		   = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

	return fmt::format("{:%Y-%m-%dT%H:%M:%S}.{:03d}Z", fmt::gmtime(in_time_t), static_cast<int>(ms.count()));
}

#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

static std::string pictureTypeToString(int type) {
	std::vector<std::string> parts;

	if (type & 1) parts.push_back("Fire");
	if (type & 2) parts.push_back("Background");

	if (parts.empty()) return "None";

	std::string result;
	for (size_t i = 0; i < parts.size(); ++i) {
		if (i > 0) {
			if (result.length() + parts[i].length() + 1 > 38) {
				result += "+";
				break;
			}
			result += ",";
		}
		result += parts[i];
	}

	return result;
}

static int64_t getCurrentTimestamp() {
	auto now = std::chrono::system_clock::now();

	auto duration = now.time_since_epoch();
	return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

static std::string get_guid() {
	auto &dev = DeviceInfo::instance();
	return dev.deviceId();
}

cv::Rect extractUpperBodyRegion(std::shared_ptr<AXVideoFrame> frame, ax_bbox_t bbox, std::shared_ptr<HwIvps> ivps, int alignment = 128) {
	if (nullptr == frame) return {};

	// 1. expand the width and height of the face by 2x
	float width_scale  = 2.0f;	// the width has been expanded by 2x
	float height_scale = 2.0f;	// the height is 2x larger

	int w = frame->width();
	int h = frame->height();

	int new_width  = static_cast<int>(bbox.w * width_scale);
	int new_height = static_cast<int>(bbox.h * height_scale);

	// 2. will be highly extended(increase 1.5x)
	new_height = static_cast<int>(new_height * 1.5f);

	// 3. adjusted to 3:4 proportion
	// calculate the target width which is the height of the target width 3:4
	float target_width = new_height * 3.0f / 4.0f;

	if (new_width < target_width) {
		// if the expanded width is less than the target width,then adjust according to the target width
		new_width = static_cast<int>(target_width);
	} else {
		// if the expanded width is greater than the target width,then the height is adjusted according to the width
		new_height = static_cast<int>(new_width * 4.0f / 3.0f);
	}

	// 4. align to a specified multiple(defaults to128)
	new_width  = (new_width + alignment - 1) / alignment * alignment;	// round up to the specified multiple
	new_height = (new_height + alignment - 1) / alignment * alignment;	// round up to the specified multiple

	// calculates the upper left coordinates
	int center_x = bbox.x + bbox.w / 2;
	int center_y = bbox.y + bbox.h / 2;

	// make sure that the top left coordinates are so that the box doesn t go out of bounds
	int x = center_x - new_width / 2;
	int y = center_y - new_height / 2;

	// limit coordinates and the size of the box,so that the box does not extend beyond the image
	x		   = std::max(0, x);
	y		   = std::max(0, y);
	new_width  = std::min(new_width, w - x);
	new_height = std::min(new_height, h - y);

	cv::Rect body_rect(x, y, new_width, new_height);
	return body_rect;  // retrun to the updated box
}

std::shared_ptr<AXVideoFrame> frame_crop(std::shared_ptr<AXVideoFrame> frame, ax_bbox_t bbox, std::shared_ptr<HwIvps> ivps, std::shared_ptr<HwCapture> Capture, int alignment = 128) {
	auto body_rect = extractUpperBodyRegion(frame, ax_bbox_t{bbox.x, bbox.y, bbox.w, bbox.h}, ivps);
	if (body_rect.area() > 1024) {
		// cv::Rect clipped_rect = cv::Rect(0, 0, frame->width(), frame->height()) & cv::Rect(ibox.rect);
		auto jpeg = ivps->HwIvpsCropResize(frame->raw(), {body_rect.width, body_rect.height},
										   {body_rect.x, body_rect.y, body_rect.width, body_rect.height}, IVPS_ENGINE_ID_TDP);
		return jpeg;
	} else {
		return nullptr;
	}
}

//
inline void printRow(const std::string &key, const std::string &value) {
	fmt::print("│ {:<20} │ {:<40} │\n", key, value);
}
inline void printRow(const std::string &key, int value) {
	fmt::print("│ {:<20} │ {:<40d} │\n", key, value);
}
inline void printRow(const std::string &key, double value) {
	fmt::print("│ {:<20} │ {:<40.2f} │\n", key, value);
}

// fireDetectNode
void printNodeParams(const std::string &node_name, const FireNodeParams &p) {
	fmt::print("\n┌──────────────────────┬──────────────────────────────────────────┐\n");
	fmt::print("│ {:^65} │\n", "fireDetectNode Parameter Initialization");
	fmt::print("├──────────────────────┼──────────────────────────────────────────┤\n");

	printRow("Node Name", node_name);
	printRow("Threshold", p.threshold);
	printRow("Show Fire", p.show_fire ? "Enabled" : "Disabled");
	printRow("Push Enable", p.alarm_push_enable ? "Enabled" : "Disabled");
	printRow("Alarm Interval", p.alarm_push_interval);
	printRow("APP Push", p.alarm_app_push);
	printRow("Rec Pic Type", pictureTypeToString(p.record_pic_type));
	printRow("Alarm Relay", p.alarm_relay_enable ? "Enabled" : "Disabled");
	printRow("Relay Interval", p.alarm_relay_interval);
	printRow("Device ID", p.device_id);
	printRow("Model File", p.model_path);
	printRow("Task ID", p.task_id);

	fmt::print("└──────────────────────┴──────────────────────────────────────────┘\n");
}

/* -------------------------------- ctor / dtor ----------------------------- */
fireDetectNode::fireDetectNode(std::string node_name, std::unique_ptr<FireNodeParams> params)
	: nodeParams_(std::move(params)) {
	printNodeParams(node_name, *nodeParams_);

	device_id_ = nodeParams_->device_id;
	task_id_   = nodeParams_->task_id;

	SafeAlgorithm::Options opt{ax_model_type_fire_smoke, nodeParams_->model_path, nodeParams_->device_id};
	if (alg_ = std::make_shared<SafeAlgorithm>(opt); !alg_ || !alg_->ok()) {
		fmt::print(fg(fmt::color::red), "❌ fireDetectNode algo init failed!\n");
		if (!alg_->ok()) {
			alg_->close();
			alg_.reset();
		}
		fmt::print("⚠️ fireDetectNode constructed! BUILD TIME: {} {}\n", __DATE__, __TIME__);
		return;
	}
	alg_->set_affinity(true);
	alg_->update_params([&](auto &p) {
		p.det_threshold							= 0.8f;
		p.fire_smoke_param.fire_smoke_threshold = nodeParams_->threshold;
	});

	ivps_	 = std::make_shared<HwIvps>(device_id_, 0, 0);
	Capture_ = std::make_shared<HwCapture>(device_id_);

	// last_report_time = std::chrono::steady_clock::now();
	reporter_.set_algorithm_config({task_id_,
									PLUGIN_NODE_NAME,
									std::to_string(nodeParams_->threshold)});
	fmt::print("✅ fireDetectNode constructed! BUILD TIME: {} {}\n", __DATE__, __TIME__);
}

fireDetectNode::~fireDetectNode() {
	stop();
	fmt::print("✅ fireDetectNode destructed!\n");
}

void fireDetectNode::stop() {
	set_alive(false);
	std::lock_guard<std::mutex> lk(mutex_);
	if (alg_) {
		alg_->close();
		alg_.reset();
	}
	if (ivps_) {
		ivps_.reset();
	}
	if (Capture_) {
		Capture_.reset();
	}
	fmt::print("✅ fireDetectNode stop ok!\n");
}

void fireDetectNode::render_fn(std::shared_ptr<AXVideoFrame> &canvas,
							   const std::any				 &result_any,
							   const std::any				 &extra) {
	auto draw = [&](const ax_result_t &det, std::shared_ptr<HwIvps> ivps) {
		for (int i = 0; i < det.n_objects; ++i) {
			const auto &ibox  = det.objects[i];
			AX_U32		color = random_color(ibox.track_id);
			if (ivps) {
				cv::Rect clip = cv::Rect(0, 0, canvas->width(), canvas->height()) &
								cv::Rect(ibox.bbox.x, ibox.bbox.y, ibox.bbox.w, ibox.bbox.h);
				if (clip.area() <= 200) continue;
				ivps->HwDrawRect(canvas->raw(),
								 {clip.x, clip.y, clip.width, clip.height},
								 color);
			}
		}
	};

	// best-effort get ivps from extra
	std::shared_ptr<HwIvps> ivps = nullptr;
	if (auto p = anyx::get<std::shared_ptr<HwIvps>>(extra)) {
		ivps = *p;
	}

	// visit only if any holds ax_result_t; otherwise warn and return
	if (!anyx::visit<ax_result_t>(result_any, [&](const ax_result_t &det) { draw(det, ivps); })) {
		fmt::print("⚠️ [{}] unexpected any type: {} \r\n", PLUGIN_NODE_NAME, anyx::type_name(result_any));
	}
}

std::pair<nlohmann::json,
		  std::vector<std::function<std::shared_ptr<AXVideoFrame>()>>>
fireDetectNode::alarm_fn(const std::any &result_any,
						 std::shared_ptr<AXVideoFrame>) {
	std::unordered_map<unsigned long, TrackInfo> value;

	bool ok = anyx::visit<std::unordered_map<unsigned long, TrackInfo>>(result_any, [&](const auto &v) { value = v; });
	if (!ok || value.empty()) {
		if (!ok) fmt::print("⚠️ [{} Alarm] unexpected any type: {} \r\n", PLUGIN_NODE_NAME, anyx::type_name(result_any));
		return {{}, {}};
	}

	nlohmann::json												alarm_arr = nlohmann::json::array();
	std::vector<std::function<std::shared_ptr<AXVideoFrame>()>> snaps;				// take multiple snapshots
	std::string													alarm_summary_str;	// it is used to stitch together each alarm message
	bool														alarm_push_enable = nodeParams_->alarm_push_enable;

	// Common image processing function
	auto addImageData = [&](const auto &snapshot, auto &json, const std::string &msg_type, const std::string &key) {
		if (snapshot) {
			json[msg_type]["data"][key] = frameToBase64(Capture_->capture(snapshot->raw()));
		}
	};

	// Process a single alarm record
	auto processAlarmRecord = [&](const auto &tr) -> std::optional<nlohmann::json> {
		if (tr.reported || !alarm_push_enable) {
			return std::nullopt;
		}

		nlohmann::json j = {
			{"local_push_msg", {}},
			{"alarm_push_msg", {{"data", {}}}}};

		// 1-1. Process local push data
		{
			j["local_push_msg"] = {
				{"alarm_type", "fire_smoke"},
				{"bbox", {{"x", tr.bbox.x}, {"y", tr.bbox.y}, {"w", tr.bbox.w}, {"h", tr.bbox.h}}}};
			
			// TTS audio pillar announcement configuration (managed independently by this plugin, not dependent on other plugins)
			if (nodeParams_->tts_fire.enabled && nodeParams_->tts_fire.hasContent()) {
				if (nodeParams_->tts_fire.isUrlMode()) {
					j["local_push_msg"]["tts_url"] = nodeParams_->tts_fire.url;
				} else {
					j["local_push_msg"]["tts_text"] = nodeParams_->tts_fire.text;
				}
			}

			// Process image snapshots
			auto processSnapshots = [&](auto type, const auto &snapshot) {
				if (nodeParams_->record_pic_type & static_cast<int>(type) && snapshot) {
					snaps.push_back([snapshot]() { return snapshot; });
				}
			};

			processSnapshots(PicType::FIRE, tr.fire_snapshot);
			processSnapshots(PicType::BACKGROUND, tr.bg_snapshot);
		}

		// 1-2. Process alarm push data
		{
			j["alarm_push_msg"] = {
				{"did", get_guid()},
				{"type", static_cast<int>(EventType::FIRE_ALARM)},
				{"time", getCurrentTimestamp()},
				{"state", 0}};

			addImageData(tr.bg_snapshot, j, "alarm_push_msg", "bg_data");
		}

		return std::make_optional(std::move(j));
	};

	/* ---------- 1. Check each result in turn ---------- */
	for (auto &[key, tr] : value) {
		if (std::optional<nlohmann::json> processed = processAlarmRecord(tr)) {
			alarm_arr.push_back(std::move(*processed));
		}
	}

	if (alarm_arr.empty())
		return {{}, {}};

	/* ---------- 2. root json ---------- */
	nlohmann::json root;
	root["msg"] =
		"TaskId:" + this->task_id_ + " " +
		jdk_node_base::node_name() +
		" AlarmType: fireAlarm Timestamp:" +
		get_current_iso_time();

	root["alarm_type"]	   = "fireAlarm";
	root["alarm_push_uri"] = alarm_push_enable ? "/api/v1/device/report/event" : "";
	root["timestamp"]	   = get_current_iso_time();
	root["alarm_count"]	   = alarm_arr.size();
	root["alarms"]		   = std::move(alarm_arr);

	if (nodeParams_->llm_review.enabled) {
		fmt::print(
			"[FIRE][llm_request] task_id={} expected={} timeout_ms={} decode_location={}\n",
			task_id_,
			nodeParams_->llm_review.expected_keyword,
			nodeParams_->llm_review.timeout_ms,
			nodeParams_->llm_review.decode_location.empty() ? "inherit_llm_profile" : nodeParams_->llm_review.decode_location);
		nlohmann::json llm_req = {
			{"enabled", true},
			{"prompt", nodeParams_->llm_review.prompt},
			{"expected_keyword", nodeParams_->llm_review.expected_keyword},
			{"timeout_ms", nodeParams_->llm_review.timeout_ms},
			{"deny_on_mismatch", nodeParams_->llm_review.deny_on_mismatch},
			{"follow_upstream_on_error", nodeParams_->llm_review.follow_upstream_on_error},
				{"payload", {
					{"algo", jdk_node_base::node_name()},
					{"task_id", task_id_},
					{"alarm_type", "fireAlarm"}
				}}
			};
		if (!nodeParams_->llm_review.decode_location.empty()) {
			llm_req["decode_location"] = nodeParams_->llm_review.decode_location;
		}
		root["llm_request"] = std::move(llm_req);
	}

	return {root, snaps};
}

void fireDetectNode::run_infer_combinations(
	const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>> &batch) {
	if (!alg_) return;
	assert(batch.size() == 1);
	auto &meta	= batch[0];
	auto  frame = meta->frame_;

	// auto		hostframe = frame->toHost();
	ax_result_t det{};
	if (alg_) alg_->track(frame, det, Capture_);

	if (det.n_objects < 0 || det.n_objects > AX_ALGORITHM_MAX_OBJ_NUM) {  // protection prevent extreme anomalies
		return;
	}

	auto					   now = std::chrono::steady_clock::now();
	std::vector<std::uint64_t> pending_ids;	 // the trajectory that may be triggered in this frame
	bool					   need_fire = nodeParams_->record_pic_type & static_cast<int>(PicType::FIRE);
	bool					   need_bg	 = nodeParams_->record_pic_type & static_cast<int>(PicType::BACKGROUND) || nodeParams_->alarm_push_enable;

	std::vector<ax_bbox_t>().swap(alarms_);	 // clear previous alarms
	for (int i = 0; i < det.n_objects; ++i) {
		auto		 &obj = det.objects[i];
		unsigned long id  = obj.track_id;
		ax_bbox_t	  bb  = obj.bbox;
		// printf("Person id:%lu, x:%d, y:%d, w:%d, h:%d\r\n", id, bb.x, bb.y, bb.w, bb.h);
		auto &tr = tracks_[id];
		// memset(&tr, 0, sizeof(TrackInfo));	// reset track info
		tr.bbox	   = bb;
		tr.last_ts = now;
		if (!tr.reported) {
			pending_ids.emplace_back(id);

			if (need_fire) {
				tr.fire_snapshot = frame_crop(frame, bb, ivps_, Capture_);
			}
			if (need_bg) {
				tr.bg_snapshot = frame;
			}
		}
	}
	/* 2. cleanup timed out track */
	std::erase_if(tracks_, [&](const auto &kv) {
		return std::chrono::duration_cast<std::chrono::seconds>(now - kv.second.last_ts)
				   .count() > track_timeout_sec_;
	});

	bool need_alarm = !pending_ids.empty();	 // there is nothing to report in this frame

	auto tracks_sp = std::make_shared<std::unordered_map<unsigned long, TrackInfo>>(tracks_);
	{
		std::lock_guard<std::mutex> lk(*meta->mtx_);
		auto						new_entry = std::make_shared<jdk_objects::ResultEntry>(
			   //    std::make_shared<std::any>(det_sp),
			   anyx::any_make<ax_result_t>(det),
			   /* render_cb */
			   [this](std::shared_ptr<AXVideoFrame> &canvas,
					  const std::any &any, const std::any &extra) {
				   render_fn(canvas, any, extra);
			   },
			   /* alarm_cb */
			   [this, tracks_sp, need_alarm](const std::any &) -> std::pair<nlohmann::json, std::vector<std::function<std::shared_ptr<AXVideoFrame>()>>> {
				   if (!tracks_sp->empty() && need_alarm) {
					   return this->alarm_fn(*tracks_sp, nullptr);
				   } else {
					   return std::make_pair(nlohmann::json::object(), std::vector<std::function<std::shared_ptr<AXVideoFrame>()>>{});
				   }
			   },
			   /* push_enabled */ nodeParams_->alarm_push_enable,
			   /* push_interval_ms */ nodeParams_->alarm_push_interval * 1000);
		meta->result_map_[jdk_node_base::node_name()].exchange(new_entry);
	}
	// 2) these tracks are set only after the alarm is executed reported = true
	for (auto id : std::exchange(pending_ids, {})) {
		tracks_[id].reported = true;
		tracks_[id].fire_snapshot.reset();
		tracks_[id].bg_snapshot.reset();
	}
}

std::shared_ptr<jdk_objects::jdk_meta>
fireDetectNode::handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) {
	std::lock_guard<std::mutex> lk(mutex_);
	if (!is_alive()) return nullptr;
	if (!meta || (!meta->frame_)) {
		std::cerr << "❌ meta is null or contains no valid frame.\n";
		return jdk_node_base::handle_frame_meta(meta);
	}
	std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>> batch{meta};

	// Automatic timing, write the time spent to reporter_ at the end of the function.
	MetricsReporter::ScopedTimer timer(reporter_);

	run_infer_combinations(batch);

	// report node info
	reporter_.report_algorithm(jdk_node_base::node_fps(), meta->create_time);
	return jdk_node_base::handle_frame_meta(meta);
}

void fireDetectNode::handle_frame_meta(
	const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>> &batch) {
	run_infer_combinations(batch);
}

std::shared_ptr<jdk_objects::jdk_meta> fireDetectNode::handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) {
	if (!meta) return nullptr;
	std::cout << "[fireDetectNode] handle_control_meta: control_type = " << meta->control_type << std::endl;
	if (meta->control_type == jdk_objects::jdk_control_type::SPEAK) this->stop();

	return meta;
};

}  // namespace jdk_nodes
