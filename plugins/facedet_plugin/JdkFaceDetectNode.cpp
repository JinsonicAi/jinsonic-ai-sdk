#include "JdkFaceDetectNode.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "DevProtoDef.hpp"
#include "DeviceInfo.hpp"
#include "HwIvps.hpp"
#include "JdkOsd.hpp"
#include "alg_comm.hpp"
#include "anyx.hpp"
#include "post_node_info.h"

namespace jdk_nodes {
faceDetectV2Node::faceDetectV2Node(std::string node_name,
								   const std::string& filename,
								   float threshold,
								   int device_id,
								   std::string task_id,
								   int label_score_step)
	: channel_id_(device_id),
	  threshold_(threshold),
	  device_id_(device_id),
	  label_score_step_(std::clamp(label_score_step, 0, 100)),
	  task_id_(task_id) {
	infer = YOLOFACE::create_infer(filename, "ax", device_id_);
	reporter_.set_algorithm_config({task_id_,
									PLUGIN_NODE_NAME,
									"-"});
	fmt::print("✅ faceDetectV2Node constructed! BUILD TIME: {} {}\n", __DATE__, __TIME__);
}

faceDetectV2Node::~faceDetectV2Node() {
	stop();
	fmt::print("✅ faceDetectV2Node destructed!\n");
}

void faceDetectV2Node::stop() {
	set_alive(false);
	std::lock_guard<std::mutex> lk(mutex_);

	if (infer) {
		infer.reset();
	}
	fmt::print("✅ faceDetectV2Node stop ok!\n");
}

void faceDetectV2Node::render_fn(std::shared_ptr<AXVideoFrame>& canvas,
								 const std::any&				result_any,
								 const std::any&				extra) {
	const auto&				payload_any = jdk_osd::payload_from_any(result_any);
	std::shared_ptr<HwIvps> ivps		= nullptr;
	if (auto p = anyx::get<std::shared_ptr<HwIvps>>(extra))
		ivps = *p;

	YOLOFACE::Objects det;
	bool			  ok = anyx::visit<YOLOFACE::Objects>(payload_any, [&](const auto& v) { det = v; });

	if (ivps) {
		for (const auto& b : det) {
			cv::Rect clipped = cv::Rect(0, 0, canvas->width(), canvas->height()) &
							   cv::Rect((int)b.rect.x, (int)b.rect.y, (int)b.rect.width, (int)b.rect.height);

			if (clipped.area() <= 200)
				continue;
			ivps->HwDrawRect(canvas->raw(),
							 {clipped.x, clipped.y, clipped.width, clipped.height},
							 /*color*/ 0);
		}
	}
}

jdk_osd::Overlay faceDetectV2Node::build_overlay_(const YOLOFACE::Objects& det) {
	jdk_osd::Overlay overlay;

	overlay.boxes.reserve(det.size());
	for (const auto& b : det) {
		if (b.rect.width * b.rect.height <= 200)
			continue;
		jdk_osd::Box box;
		bool		 is_ghost	  = false;
		box.x					  = b.rect.x;
		box.y					  = b.rect.y;
		box.w					  = b.rect.width;
		box.h					  = b.rect.height;
		box.score				  = b.prob;
		box.track_id			  = 0;
		box.ghost				  = is_ghost;
		box.priority			  = is_ghost ? 10 : 100;
		const AX_U32 color_rgb	  = random_color(box.track_id ? static_cast<int>(box.track_id) : b.label);
		box.style.color			  = is_ghost ? jdk_osd::Color::from_rgb(color_rgb, 180)
											 : jdk_osd::Color::from_rgb(color_rgb, 255);
		box.style.thickness		  = 0;
		box.style.alpha			  = box.style.color.a;
		box.label_style.font_size = 22;
		box.label_style.fg		  = jdk_osd::Color{255, 255, 255, 255};
		box.label_style.bg		  = box.style.color;
		box.label_style.bg_alpha  = box.style.color.a;
		int score_pct = static_cast<int>(std::round(std::clamp(b.prob, 0.0f, 1.0f) * 100.0f));
		if (label_score_step_ > 1) {
			score_pct = ((score_pct + label_score_step_ / 2) / label_score_step_) * label_score_step_;
			score_pct = std::clamp(score_pct, 0, 100);
		}
		box.label				  = fmt::format("face {}%", score_pct);
		overlay.boxes.push_back(std::move(box));
	}

	for (const auto& face : det) {
		for (const auto& pt : face.landmarks) {
			jdk_osd::Keypoint kp;
			kp.p			   = {pt.x, pt.y};
			kp.radius		   = 2;
			kp.style.color	   = jdk_osd::Color{0, 210, 255, 255};
			kp.style.thickness = 1;
			kp.priority		   = 80;
			overlay.keypoints.push_back(kp);
		}
	}
	return overlay;
}

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
	return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

static std::string get_guid() {
	auto& dev = DeviceInfo::instance();
	return dev.deviceId();
}

std::pair<nlohmann::json, std::vector<std::function<std::shared_ptr<AXVideoFrame>()>>>
faceDetectV2Node::alarm_fn(const std::any& result_any, std::shared_ptr<AXVideoFrame> canvas) {
	(void)canvas;
	const auto&		  payload_any = jdk_osd::payload_from_any(result_any);
	YOLOFACE::Objects value;

	bool ok = anyx::visit<YOLOFACE::Objects>(payload_any, [&](const auto& v) { value = v; });
	if (!ok || value.empty()) {
		if (!ok)
			fmt::print("⚠️ [{} Alarm] unexpected any type: {} \r\n", PLUGIN_NODE_NAME, anyx::type_name(result_any));
		return {{}, {}};
	}

	constexpr const char* kAlarmType = "faceAlarm";
	const int64_t		  now_ms	 = getCurrentTimestamp();
	const std::string	  now_iso	 = get_current_iso_time();

	auto to_bbox = [](const YOLOFACE::FaceBox& box) {
		return nlohmann::json{
			{"x", box.rect.x},
			{"y", box.rect.y},
			{"w", box.rect.width},
			{"h", box.rect.height}};
	};

	auto to_landmarks = [](const YOLOFACE::FaceBox& box) {
		nlohmann::json arr = nlohmann::json::array();
		for (const auto& pt : box.landmarks) {
			arr.push_back({{"x", pt.x}, {"y", pt.y}});
		}
		return arr;
	};

	auto build_alarm_item = [&](const YOLOFACE::FaceBox& box) -> nlohmann::json {
		const auto bbox		 = to_bbox(box);
		const auto landmarks = to_landmarks(box);

		nlohmann::json one = {
			{"local_push_msg", {{"alarm_type", kAlarmType}, {"confidence", box.prob}, {"label", box.label}, {"bbox", bbox}, {"landmarks", landmarks}}},
			{"alarm_push_msg", {{"did", get_guid()}, {"type", static_cast<int>(EventType::HUMAN_DETECTION)}, {"time", now_ms}, {"state", 0}, {"data", {{"confidence", box.prob}, {"label", box.label}, {"bbox", bbox}, {"landmarks", landmarks}}}}}};
		return one;
	};

	nlohmann::json alarm_arr = nlohmann::json::array();
	for (const auto& box : value) {
		if (box.prob < threshold_)
			continue;
		alarm_arr.push_back(build_alarm_item(box));
	}
	if (alarm_arr.empty()) {
		return {{}, {}};
	}

	nlohmann::json root = {
		{"msg", "TaskId:" + this->task_id_ + " " +
					jdk_node_base::node_name() + " AlarmType: " + kAlarmType +
					" Timestamp:" + now_iso},
		{"alarm_type", kAlarmType},
		{"alarm_push_uri", "/api/v1/device/report/event"},
		{"timestamp", now_iso},
		{"alarm_count", alarm_arr.size()},
		{"alarms", std::move(alarm_arr)}};

	return {root, {}};
}

void faceDetectV2Node::run_infer_combinations(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& frame_meta_with_batch) {
	assert(frame_meta_with_batch.size() == 1);
	auto& frame_meta = frame_meta_with_batch[0];
	auto  result_any = infer->commit(frame_meta->frame_).get();
	auto  result_sp	 = anyx::any_try_cast<YOLOFACE::Objects>(result_any);
	if (!result_sp) {
		fmt::print("⚠️ FaceDet skip: got {}\n", anyx::type_name(result_any));
		return;
	}
	YOLOFACE::Objects det;
	anyx::visit<YOLOFACE::Objects>(*result_sp, [&](const auto& v) { det = v; });
	auto overlay_result = jdk_osd::make_overlay_result(result_sp, build_overlay_(det));
	{
		std::lock_guard<std::mutex> lk(*frame_meta->mtx_);
		auto						new_entry = std::make_shared<jdk_objects::ResultEntry>(
			overlay_result,
			[this](std::shared_ptr<AXVideoFrame>& canvas, const std::any& future_any, const std::any& extra) {
				return this->render_fn(canvas, future_any, extra);
			},
			[this](const std::any& input)
				-> std::pair<nlohmann::json, std::vector<std::function<std::shared_ptr<AXVideoFrame>()>>> {
				return this->alarm_fn(input, nullptr);
			},
			true, 10000);
		frame_meta->result_map_[jdk_node_base::node_name()].exchange(new_entry);
	}
}

// handle frame meta one by one
std::shared_ptr<jdk_objects::jdk_meta> faceDetectV2Node::handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) {
	std::lock_guard<std::mutex> lk(mutex_);
	if (!is_alive())
		return nullptr;
	if (!meta || (!meta->frame_)) {
		std::cerr << "❌ meta is null or contains no valid frame.\n";
		return jdk_node_base::handle_frame_meta(meta);
	}
	std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>> frame_meta_with_batch{meta};

	// Automatic timing, write the time spent to reporter_ at the end of the function.
	MetricsReporter::ScopedTimer timer(reporter_);
	run_infer_combinations(frame_meta_with_batch);
	// report node info
	reporter_.report_algorithm(jdk_node_base::node_fps(), meta->create_time);
	return jdk_node_base::handle_frame_meta(meta);
}

void faceDetectV2Node::handle_frame_meta(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& meta_with_batch) {
	const auto& frame_meta_with_batch = meta_with_batch;
	run_infer_combinations(frame_meta_with_batch);
}

std::shared_ptr<jdk_objects::jdk_meta> faceDetectV2Node::handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) {
	if (!meta)
		return nullptr;
	std::cout << "[faceDetectV2Node] handle_control_meta: control_type = " << meta->control_type << std::endl;
	if (meta->control_type == jdk_objects::jdk_control_type::SPEAK)
		this->stop();

	return meta;
};

}  // namespace jdk_nodes
