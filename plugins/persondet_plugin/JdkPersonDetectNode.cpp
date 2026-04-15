#include "JdkPersonDetectNode.hpp"

#include <iomanip>
#include <optional>
#include <utility>

#include "DevProtoDef.hpp"
#include "DeviceInfo.hpp"
#include "HwIvps.hpp"
#include "alg_comm.hpp"
#include "anyx.hpp"
#include "base64.hpp"
#include "post_node_info.h"

namespace jdk_nodes {

std::string get_current_iso_time() {
	using namespace std::chrono;

	auto now	   = system_clock::now();
	auto in_time_t = system_clock::to_time_t(now);
	auto ms		   = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

	std::ostringstream ss;
	ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%S");
	ss << '.' << std::setw(3) << std::setfill('0') << ms.count() << "Z";
	return ss.str();
}

static int64_t getCurrentTimestamp() {
	auto now = std::chrono::system_clock::now();

	auto duration = now.time_since_epoch();
	return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

static std::string get_guid() {
	auto& dev = DeviceInfo::instance();
	return dev.deviceId();
}

#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

void printRegions(const std::vector<std::vector<cv::Point>>& regions) {
	for (std::size_t i = 0; i < regions.size(); ++i) {
		std::cout << "区域 " << i << " : ";
		for (const auto& pt : regions[i]) {
			std::cout << '(' << pt.x << ", " << pt.y << ") ";
		}
		std::cout << '\n';
	}
}

cv::Rect extractUpperBodyRegion(std::shared_ptr<AXVideoFrame> frame, ax_bbox_t bbox, std::shared_ptr<HwIvps> ivps, int alignment = 128) {
	if (nullptr == frame)
		return {};

	// 1. expand the width and height of the face by 2x
	float width_scale  = 2.0f;	// the width has been expanded by 2x
	float height_scale = 2.0f;	// the height is 2x larger

	int w = frame->width();
	int h = frame->height();

	int new_width  = static_cast<int>(bbox.w * width_scale);
	int new_height = static_cast<int>(bbox.h * height_scale);

	// 2. will be highly extended (increase 1.5x)
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

static std::string pictureTypeToString(int type) {
	std::vector<std::string> parts;

	if (type & 1)
		parts.push_back("Face");
	if (type & 2)
		parts.push_back("Body");
	if (type & 4)
		parts.push_back("Background");

	if (parts.empty())
		return "None";

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

inline void printRow(const std::string& k, const std::string& v) {
	fmt::print("│ {:<20} │ {:<40} │\n", k, v);
}
inline void printRow(const std::string& k, int v) {
	fmt::print("│ {:<20} │ {:<40d} │\n", k, v);
}
inline void printRow(const std::string& k, double v) {
	fmt::print("│ {:<20} │ {:<40.2f} │\n", k, v);
}
inline void printRow(const std::string& k, bool v) {
	fmt::print("│ {:<20} │ {:<40} │\n", k, v ? "Enabled" : "Disabled");
}

inline void printPersonDetectParams(const std::string&	   node_name,
									const PersonDetParams& p) {
	fmt::print("\n┌──────────────────────┬──────────────────────────────────────────┐\n");
	fmt::print("│ {:^66} │\n", "personDetectNode Parameter Initialization");
	fmt::print("├──────────────────────┼──────────────────────────────────────────┤\n");

	printRow("Node Name", node_name);
	printRow("Threshold", p.threshold);

	printRow("Show Face", p.show_face);
	printRow("Show Human", p.show_human);

	printRow("Mini Face", p.mini_face);
	printRow("Quality Face", p.quality_face);

	printRow("Mini Human", p.mini_human);
	printRow("Quality Human", p.quality_human);

	printRow("Cap Mode", p.cap_mode);
	printRow("Cap Interval", p.cap_interval);

	printRow("Push Enable", p.alarm_push_enable);
	printRow("Push Interval", p.alarm_push_interval);
	printRow("APP Push", p.alarm_app_push);

	printRow("Server Push", p.server_push_enable);
	printRow("Srv Pic Type", pictureTypeToString(p.server_pic_type));

	printRow("Server Attr", p.server_attr_enable);
	printRow("Rec Pic Type", pictureTypeToString(p.record_pic_type));

	printRow("Alarm Relay", p.alarm_relay_enable);
	printRow("Relay Interval", p.alarm_relay_interval);

	printRow("Device ID", p.device_id);
	printRow("Model File", p.model_path);
	printRow("Task ID", p.task_id);

	fmt::print("└──────────────────────┴──────────────────────────────────────────┘\n");
}

void personDetectNode::init_region_analyzer() {
	// Only configure rules when there is at least one drawn region.
	if (regions_.empty()) {
		fmt::print("[persondet] no region configured, fallback to full-frame presence alarm mode.\n");
		return;
	}

	region::RegionRule rule = nodeParams_->region_rule;
	if (rule.min_bbox_size <= 0.f) {
		rule.min_bbox_size = static_cast<float>(nodeParams_->mini_human);
	}
	analyzer_.set_track_timeout(nodeParams_->region_track_timeout_sec);

	int rid	 = 1;
	int twid = 1;
	for (const auto& shape : regions_) {
		if (shape.size() >= 3) {
			region::Region region;
			region.id	   = rid++;
			region.name	   = "persondet_region_" + std::to_string(region.id);
			region.polygon = shape;
			analyzer_.add_region(std::move(region), rule);
		} else if (shape.size() == 2) {
			region::Tripwire tw;
			tw.id	= twid++;
			tw.name = "persondet_tripwire_" + std::to_string(tw.id);
			tw.p1	= cv::Point2f(static_cast<float>(shape[0].x), static_cast<float>(shape[0].y));
			tw.p2	= cv::Point2f(static_cast<float>(shape[1].x), static_cast<float>(shape[1].y));
			analyzer_.add_tripwire(std::move(tw), rule);
		}
	}

	analyzer_.set_callback([this](const region::RegionEvent& ev) {
		switch (ev.type) {
		case region::EventType::Enter:
		case region::EventType::Leave:
		case region::EventType::Loiter:
		case region::EventType::LineCrossIn:
		case region::EventType::LineCrossOut:
			pending_region_events_[ev.track_id] = ev;
			break;
		case region::EventType::Absence:
		case region::EventType::AbsenceRecover:
		case region::EventType::Crowd:
		case region::EventType::CrowdRecover:
			pending_region_global_events_.push_back(ev);
			break;
		default:
			break;
		}
	});
}

/* -------------------------------- ctor / dtor ----------------------------- */
personDetectNode::personDetectNode(std::string node_name, std::unique_ptr<PersonDetParams> params)
	: nodeParams_(std::move(params)) {
	//
	printPersonDetectParams(node_name, *nodeParams_);

	//
	device_id_ = nodeParams_->device_id;
	task_id_   = nodeParams_->task_id;

	/* for person detection */
	{
		SafeAlgorithm::Options opt{ax_model_type_person_detection, nodeParams_->model_path, nodeParams_->device_id};
		if (alg_ = std::make_shared<SafeAlgorithm>(opt); !alg_ || !alg_->ok()) {
			fmt::print(fg(fmt::color::red), "❌ {} algo init failed!\n", PLUGIN_NODE_NAME);
			if (!alg_->ok()) {
				alg_->close();
				alg_.reset();
			}
			fmt::print("⚠️ {} constructed! BUILD TIME: {} {}\n", PLUGIN_NODE_NAME, __DATE__, __TIME__);
			return;
		}
		alg_->set_affinity(true);
		alg_->update_params([&](auto& p) {
			p.det_threshold				   = nodeParams_->threshold;
			p.face_param.quality_threshold = nodeParams_->quality_face;
		});
	}

	ivps_	 = std::make_shared<HwIvps>(device_id_, 0, 0);
	Capture_ = std::make_shared<HwCapture>(device_id_);

	regions_ = getRegions(task_id_);
	printRegions(regions_);
	init_region_analyzer();
	reporter_.set_algorithm_config({task_id_,
									PLUGIN_NODE_NAME,
									std::to_string(nodeParams_->threshold)});
	fmt::print("✅ personDetectNode constructed! BUILD TIME: {} {}\n", __DATE__, __TIME__);
}

personDetectNode::~personDetectNode() {
	stop();
	fmt::print("✅ personDetectNode destructed!\n");
}

void personDetectNode::stop() {
	set_alive(false);
	std::lock_guard<std::mutex> lk(mutex_);
	tracks_.clear();
	pending_region_events_.clear();
	pending_region_global_events_.clear();
	if (alg_) {
		alg_->close();
		alg_.reset();
	}
	if (Capture_) {
		Capture_.reset();
	}
	if (ivps_) {
		ivps_.reset();
	}
	fmt::print("✅ LicensePlateRecoNode stop ok!\n");
}

void personDetectNode::render_fn(std::shared_ptr<AXVideoFrame>& canvas,
								 const std::any&				result_any,
								 const std::any&				extra) {
	auto draw = [&](const ax_result_t& det, std::shared_ptr<HwIvps> ivps) {
		for (int i = 0; i < det.n_objects; ++i) {
			const auto& ibox  = det.objects[i];
			AX_U32		color = random_color(ibox.track_id);
			if (ivps) {
				cv::Rect clip = cv::Rect(0, 0, canvas->width(), canvas->height()) &
								cv::Rect(ibox.bbox.x, ibox.bbox.y, ibox.bbox.w, ibox.bbox.h);
				if (clip.area() <= 200)
					continue;
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
	if (!anyx::visit<ax_result_t>(result_any, [&](const ax_result_t& det) { draw(det, ivps); })) {
		fmt::print("⚠️ [{}] unexpected any type: {} \r\n", PLUGIN_NODE_NAME, anyx::type_name(result_any));
	}
}

std::pair<nlohmann::json,
		  std::vector<std::function<std::shared_ptr<AXVideoFrame>()>>>
personDetectNode::alarm_fn(const std::any& result_any,
						   std::shared_ptr<AXVideoFrame>) {
	std::unordered_map<unsigned long, TrackInfo> value;
	const bool									 region_mode		 = !regions_.empty();
	const char*									 fallback_event_name = "presence";

	bool ok = anyx::visit<std::unordered_map<unsigned long, TrackInfo>>(result_any, [&](const auto& v) { value = v; });
	if (!ok || value.empty()) {
		if (!ok)
			fmt::print("⚠️ [{} Alarm] unexpected any type: {} \r\n", PLUGIN_NODE_NAME, anyx::type_name(result_any));
		return {{}, {}};
	}
	nlohmann::json												alarm_arr = nlohmann::json::array();
	std::vector<std::function<std::shared_ptr<AXVideoFrame>()>> snaps;				// take multiple snapshots
	std::string													alarm_summary_str;	// it is used to stitch together each alarm message
	bool														server_push_enable = nodeParams_->server_push_enable;
	bool														alarm_push_enable  = nodeParams_->alarm_push_enable;

	auto event_type_name = [](region::EventType type) -> const char* {
		switch (type) {
		case region::EventType::Enter:
			return "enter";
		case region::EventType::Leave:
			return "leave";
		case region::EventType::Loiter:
			return "loiter";
		case region::EventType::Absence:
			return "absence";
		case region::EventType::AbsenceRecover:
			return "absence_recover";
		case region::EventType::Crowd:
			return "crowd";
		case region::EventType::CrowdRecover:
			return "crowd_recover";
		case region::EventType::LineCrossIn:
			return "line_cross_in";
		case region::EventType::LineCrossOut:
			return "line_cross_out";
		default:
			return "unknown";
		}
	};

	// Common image processing function
	auto addImageData = [&](const auto& snapshot, auto& json, const std::string& msg_type, const std::string& key) {
		if (snapshot) {
			json[msg_type]["data"][key] = frameToBase64(Capture_->capture(snapshot->raw()));
		}
	};

	// Process a single alarm record
	auto processAlarmRecord = [&](unsigned long track_id, const auto& tr) -> std::optional<nlohmann::json> {
		if (region_mode && tr.reported) {
			return std::nullopt;
		}

		std::optional<region::RegionEvent> evt_opt;
		if (region_mode) {
			auto it_evt = pending_region_events_.find(track_id);
			if (it_evt == pending_region_events_.end()) {
				return std::nullopt;
			}
			evt_opt = it_evt->second;
		}
		const int	region_id	 = evt_opt ? evt_opt->region_id : 0;
		const int	object_count = evt_opt ? evt_opt->object_count : 1;
		const float dwell_sec	 = evt_opt ? evt_opt->dwell_sec : 0.0f;
		const float absence_sec	 = evt_opt ? evt_opt->absence_sec : 0.0f;
		const char* event_name	 = evt_opt ? event_type_name(evt_opt->type) : fallback_event_name;

		nlohmann::json j = {
			{"local_push_msg", {}},
			{"server_push_msg", {{"data", {}}}},
			{"alarm_push_msg", {{"data", {}}}}};

		// 1-1. Process local push data
		if (alarm_push_enable) {
			j["local_push_msg"] = {
				{"alarm_type", "persondet_alarm"},
				{"region_event", event_name},
				{"region_id", region_id},
				{"track_id", track_id},
				{"object_count", object_count},
				{"dwell_sec", dwell_sec},
				{"absence_sec", absence_sec},
				{"bbox", {{"x", tr.bbox.x}, {"y", tr.bbox.y}, {"w", tr.bbox.w}, {"h", tr.bbox.h}}}};

			// Process image snapshots
			auto processSnapshots = [&](auto type, const auto& snapshot) {
				if (nodeParams_->record_pic_type & static_cast<int>(type) && snapshot) {
					snaps.push_back([snapshot]() { return snapshot; });
				}
			};

			processSnapshots(PicType::FACE, tr.face_snapshot);
			processSnapshots(PicType::BODY, tr.body_snapshot);
			processSnapshots(PicType::BACKGROUND, tr.bg_snapshot);
		}

		// 1-2. Process server push data
		if (server_push_enable) {
			j["server_push_msg"] = {
				{"time", getCurrentTimestamp()},
				{"sn", get_guid()},
				{"event", event_name},
				{"region_id", region_id},
				{"track_id", track_id}};

			// Process server images
			bool need_face = nodeParams_->server_pic_type & static_cast<int>(PicType::FACE);
			bool need_body = nodeParams_->server_pic_type & static_cast<int>(PicType::BODY);
			bool need_bg   = nodeParams_->server_pic_type & static_cast<int>(PicType::BACKGROUND);

			if (need_face)
				addImageData(tr.face_snapshot, j, "server_push_msg", "face_data");
			if (need_body)
				addImageData(tr.body_snapshot, j, "server_push_msg", "body_data");
			if (need_bg)
				addImageData(tr.bg_snapshot, j, "server_push_msg", "bg_data");
		}

		// 1-3. Process alarm push data
		if (alarm_push_enable) {
			j["alarm_push_msg"] = {
				{"did", get_guid()},
				{"type", static_cast<int>(EventType::HUMAN_DETECTION)},
				{"time", getCurrentTimestamp()},
				{"state", 0},
				{"data", {{"event", event_name}, {"region_id", region_id}, {"track_id", track_id}, {"object_count", object_count}, {"dwell_sec", dwell_sec}, {"absence_sec", absence_sec}}}};

			addImageData(tr.bg_snapshot, j, "alarm_push_msg", "bg_data");
		}

		return std::make_optional(std::move(j));
	};

	/* ---------- 1. Check each result in turn ---------- */
	for (auto& [key, tr] : value) {
		if (std::optional<nlohmann::json> processed = processAlarmRecord(key, tr)) {
			alarm_arr.push_back(std::move(*processed));
		}
	}

	while (!pending_region_global_events_.empty()) {
		const auto evt = pending_region_global_events_.front();
		pending_region_global_events_.pop_front();
		nlohmann::json j = {
			{"local_push_msg", {{"alarm_type", "persondet_alarm"}, {"region_event", event_type_name(evt.type)}, {"region_id", evt.region_id}, {"track_id", 0}, {"object_count", evt.object_count}, {"dwell_sec", evt.dwell_sec}, {"absence_sec", evt.absence_sec}}},
			{"server_push_msg", {{"data", {}}, {"time", getCurrentTimestamp()}, {"sn", get_guid()}, {"event", event_type_name(evt.type)}, {"region_id", evt.region_id}, {"track_id", 0}}},
			{"alarm_push_msg", {{"did", get_guid()}, {"type", static_cast<int>(EventType::HUMAN_DETECTION)}, {"time", getCurrentTimestamp()}, {"state", 0}, {"data", {{"event", event_type_name(evt.type)}, {"region_id", evt.region_id}, {"track_id", 0}, {"object_count", evt.object_count}, {"dwell_sec", evt.dwell_sec}, {"absence_sec", evt.absence_sec}}}}}};
		alarm_arr.push_back(std::move(j));
	}

	if (alarm_arr.empty())
		return {{}, {}};

	/* ---------- 2. root json ---------- */
	nlohmann::json root;
	root["msg"] =
		"TaskId:" + this->task_id_ + " " +
		jdk_node_base::node_name() +
		" AlarmType: persondet_alarm Timestamp:" +
		get_current_iso_time();

	root["alarm_type"]		= "persondet_alarm";
	root["server_push_uri"] = server_push_enable ? "/api/v1/face/capture" : "";
	root["alarm_push_uri"]	= alarm_push_enable ? "/api/v1/device/report/event" : "";
	root["timestamp"]		= get_current_iso_time();
	root["alarm_count"]		= alarm_arr.size();
	root["alarms"]			= std::move(alarm_arr);

	if (nodeParams_->llm_review.enabled) {
		nlohmann::json llm_req = {
			{"enabled", true},
			{"prompt", nodeParams_->llm_review.prompt},
			{"expected_keyword", nodeParams_->llm_review.expected_keyword},
			{"timeout_ms", nodeParams_->llm_review.timeout_ms},
			{"deny_on_mismatch", nodeParams_->llm_review.deny_on_mismatch},
			{"follow_upstream_on_error", nodeParams_->llm_review.follow_upstream_on_error},
			{"payload", {{"algo", jdk_node_base::node_name()}, {"task_id", task_id_}, {"alarm_type", "persondet_alarm"}}}};
		if (!nodeParams_->llm_review.decode_location.empty()) {
			llm_req["decode_location"] = nodeParams_->llm_review.decode_location;
		}
		root["llm_request"] = std::move(llm_req);
	}

	return {root, snaps};
}

inline cv::Rect toCvRectI(const ax_bbox_t& b) noexcept {
	return {
		static_cast<int>(std::lround(b.x)),
		static_cast<int>(std::lround(b.y)),
		static_cast<int>(std::lround(b.w)),
		static_cast<int>(std::lround(b.h))};
}

inline bool isRectInsideRegions(const cv::Rect&							   r,
								const std::vector<std::vector<cv::Point>>& regions) {
	cv::Point2f center(r.x + r.width * 0.5f,
					   r.y + r.height * 0.5f);

	for (const auto& poly : regions)
		if (cv::pointPolygonTest(poly, center, /*measureDist=*/false) >= 0)
			return true;

	return false;
}

inline bool isBboxSmallerThanThreshold(const ax_bbox_t& bbox, float threshold) {
	return (bbox.w < threshold) && (bbox.h < threshold);
}

void personDetectNode::run_infer_combinations(
	const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& batch) {
	assert(batch.size() == 1);
	auto&		meta  = batch[0];
	auto		frame = meta->frame_;
	ax_result_t det{};
	// auto		hostframe = frame->toHost();
	if (alg_)
		alg_->track(frame, det, Capture_);
	if (det.n_objects < 0 || det.n_objects > AX_ALGORITHM_MAX_OBJ_NUM) {  // protection prevent extreme anomalies
		// printf("[PersonDet] Invalid n_objects = %d\n", det.n_objects);
		return;
	}

	auto	   now		   = std::chrono::steady_clock::now();
	const bool region_mode = !regions_.empty();
	// printf("det.n_objects:%d model_type:%d\r\n", det.n_objects, det.model_type);
	for (int i = 0; i < det.n_objects; ++i) {
		auto&		  obj = det.objects[i];
		unsigned long id  = obj.track_id;
		ax_bbox_t	  bb  = obj.bbox;
		// printf("Person id:%lu, x:%d, y:%d, w:%d, h:%d\r\n", id, bb.x, bb.y, bb.w, bb.h);
		auto [it, inserted] = tracks_.try_emplace(id);
		auto& tr			= it->second;
		if (inserted) {
			tr.reported = false;
			tr.departed = false;
		}
		tr.bbox	   = bb;
		tr.last_ts = now;
	}

	/* 2. cleanup timed out track */
	std::erase_if(tracks_, [&](const auto& kv) {
		return std::chrono::duration_cast<std::chrono::seconds>(now - kv.second.last_ts)
				   .count() > nodeParams_->region_track_timeout_sec;
	});

	analyzer_.update(det);

	std::vector<std::uint64_t> pending_ids;	 // the trajectory that may be triggered in this frame
	bool					   need_face = nodeParams_->record_pic_type & static_cast<int>(PicType::FACE);
	bool					   need_body = nodeParams_->record_pic_type & static_cast<int>(PicType::BODY);
	bool					   need_bg	 = nodeParams_->record_pic_type & static_cast<int>(PicType::BACKGROUND) || nodeParams_->alarm_push_enable;

	for (auto& [id, tr] : tracks_) {
		if (isBboxSmallerThanThreshold(tr.bbox, nodeParams_->mini_human)) {
			continue;
		}

		bool should_capture = false;
		if (!region_mode) {
			should_capture = true;
		} else {
			auto it_evt = pending_region_events_.find(id);
			if (it_evt == pending_region_events_.end()) {
				continue;
			}
			if (nodeParams_->cap_mode == static_cast<int>(CapMode::SNAPSHOT_AFTER_LEAVING)) {
				should_capture = (it_evt->second.type == region::EventType::Leave);
			} else {
				should_capture = (it_evt->second.type == region::EventType::Enter ||
								  it_evt->second.type == region::EventType::Loiter ||
								  it_evt->second.type == region::EventType::LineCrossIn ||
								  it_evt->second.type == region::EventType::LineCrossOut);
			}
		}

		if (!should_capture)
			continue;

		pending_ids.emplace_back(id);
		if (region_mode) {
			tr.reported = false;
		}

		if (need_face) {
			tr.face_snapshot = frame_crop(frame, tr.bbox, ivps_, Capture_);
		}
		if (need_body) {
			tr.body_snapshot = frame_crop(frame, tr.bbox, ivps_, Capture_);
		}
		if (need_bg) {
			tr.bg_snapshot = frame;
		}
	}

	/* 4. set interval time */
	int intervalTime;
	if (nodeParams_->cap_mode == static_cast<int>(CapMode::REAL_TIME_SNAPSHOT)) {
		intervalTime = 0;
	} else if (nodeParams_->cap_mode == static_cast<int>(CapMode::INTERVAL_SNAPSHOT)) {
		intervalTime = nodeParams_->cap_interval;
	} else {
		intervalTime = nodeParams_->alarm_push_interval;
	}

	bool need_alarm = !pending_ids.empty();	 // there is nothing to report in this frame
	auto tracks_sp	= std::make_shared<std::unordered_map<unsigned long, TrackInfo>>(tracks_);
	{
		std::lock_guard<std::mutex> lk(*meta->mtx_);
		auto						new_entry = std::make_shared<jdk_objects::ResultEntry>(
			anyx::any_make<ax_result_t>(det),
			/* render_cb */
			[this](std::shared_ptr<AXVideoFrame>& canvas,
				   const std::any& any, const std::any& extra) {
				render_fn(canvas, any, extra);
			},
			/* alarm_cb */
			[this, tracks_sp, need_alarm](const std::any&) -> std::pair<nlohmann::json, std::vector<std::function<std::shared_ptr<AXVideoFrame>()>>> {
				if (!tracks_sp->empty() && need_alarm) {
					return this->alarm_fn(*tracks_sp, nullptr);
				} else {
					return std::make_pair(nlohmann::json::object(), std::vector<std::function<std::shared_ptr<AXVideoFrame>()>>{});
				}
			},
			/* push_enabled */ true,
			/* push_interval_ms */ intervalTime * 1000);
		meta->result_map_[jdk_node_base::node_name()].exchange(new_entry);
	}
	// 2) these tracks are set only after the alarm is executed reported = true
	for (auto id : std::exchange(pending_ids, {})) {
		if (region_mode) {
			tracks_[id].reported = true;
		}
		tracks_[id].face_snapshot.reset();
		tracks_[id].body_snapshot.reset();
		tracks_[id].bg_snapshot.reset();
		if (region_mode) {
			pending_region_events_.erase(id);
		}
	}
}

std::shared_ptr<jdk_objects::jdk_meta>
personDetectNode::handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) {
	std::lock_guard<std::mutex> lk(mutex_);
	if (!is_alive())
		return nullptr;
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

void personDetectNode::handle_frame_meta(
	const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& batch) {
	run_infer_combinations(batch);
}

std::shared_ptr<jdk_objects::jdk_meta> personDetectNode::handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) {
	if (!meta)
		return nullptr;
	std::cout << "[personDetectNode] handle_control_meta: control_type = " << meta->control_type << std::endl;
	if (meta->control_type == jdk_objects::jdk_control_type::SPEAK)
		this->stop();

	return meta;
};

}  // namespace jdk_nodes
