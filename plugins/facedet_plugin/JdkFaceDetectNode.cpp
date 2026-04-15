#include "JdkFaceDetectNode.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "DevProtoDef.hpp"
#include "DeviceInfo.hpp"
#include "HwIvps.hpp"
#include "alg_comm.hpp"
#include "anyx.hpp"
#include "post_node_info.h"

namespace jdk_nodes {
faceDetectNode::faceDetectNode(std::string node_name, const std::string& filename, float threshold, int device_id, std::string task_id)
	: channel_id_(device_id), threshold_(threshold), device_id_(device_id), task_id_(task_id) {
	infer = YOLOFACE::create_infer(filename, "ax", device_id_);
	reporter_.set_algorithm_config({task_id_,
									PLUGIN_NODE_NAME,
									"-"});
	fmt::print("✅ faceDetectNode constructed! BUILD TIME: {} {}\n", __DATE__, __TIME__);
}

faceDetectNode::~faceDetectNode() {
	stop();
	fmt::print("✅ faceDetectNode destructed!\n");
}

void faceDetectNode::stop() {
	set_alive(false);
	std::lock_guard<std::mutex> lk(mutex_);

	if (infer) {
		infer.reset();
	}
	fmt::print("✅ faceDetectNode stop ok!\n");
}

#if 0
void faceDetectNode::render_fn(std::shared_ptr<AXVideoFrame>& canvas, const std::any& result_any, const std::any& extra) {
	auto draw = [&](const FaceDetTarget& target, std::shared_ptr<HwIvps> ivps) {
		for (const auto& ibox : target) {
			AX_U32 color = random_color(ibox.label);
			if (ivps) {
				cv::Rect clipped_rect = cv::Rect(0, 0, canvas->width(), canvas->height()) & cv::Rect(ibox.rect);
				if (clipped_rect.area() <= 200) continue;
				ivps->HwDrawRect(canvas->raw(), {clipped_rect.x, clipped_rect.y, clipped_rect.width, clipped_rect.height}, color);
			}
		}
	};

	// best-effort get ivps from extra
	std::shared_ptr<HwIvps> ivps = nullptr;
	if (auto p = anyx::get<std::shared_ptr<HwIvps>>(extra)) {
		ivps = *p;
	}

	// visit only if any holds ax_result_t; otherwise warn and return
	if (!anyx::visit<YOLOFACE::Objects>(result_any, [&](const YOLOFACE::Objects& det) { draw(det, ivps); })) {
		fmt::print("⚠️ [{}  Render] unexpected any type: {} \r\n", PLUGIN_NODE_NAME, anyx::type_name(result_any));
	}
}
#endif

void faceDetectNode::render_fn(std::shared_ptr<AXVideoFrame>& canvas,
							   const std::any&				  result_any,
							   const std::any&				  extra) {
	std::shared_ptr<HwIvps> ivps = nullptr;
	if (auto p = anyx::get<std::shared_ptr<HwIvps>>(extra))
		ivps = *p;

	YOLOFACE::Objects det;
	bool			  ok = anyx::visit<YOLOFACE::Objects>(result_any, [&](const auto& v) { det = v; });

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
faceDetectNode::alarm_fn(const std::any& result_any, std::shared_ptr<AXVideoFrame> canvas) {
	(void)canvas;
	YOLOFACE::Objects value;

	bool ok = anyx::visit<YOLOFACE::Objects>(result_any, [&](const auto& v) { value = v; });
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

void faceDetectNode::run_infer_combinations(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& frame_meta_with_batch) {
	assert(frame_meta_with_batch.size() == 1);
	auto& frame_meta = frame_meta_with_batch[0];
	auto  result_any = infer->commit(frame_meta->frame_).get();
	auto  result_sp	 = anyx::any_try_cast<YOLOFACE::Objects>(result_any);
	if (!result_sp) {
		fmt::print("⚠️ FaceDet skip: got {}\n", anyx::type_name(result_any));
		return;
	}
	{
		std::lock_guard<std::mutex> lk(*frame_meta->mtx_);
		auto						new_entry = std::make_shared<jdk_objects::ResultEntry>(
			result_sp,
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
std::shared_ptr<jdk_objects::jdk_meta> faceDetectNode::handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) {
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

void faceDetectNode::handle_frame_meta(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& meta_with_batch) {
	const auto& frame_meta_with_batch = meta_with_batch;
	run_infer_combinations(frame_meta_with_batch);
}

std::shared_ptr<jdk_objects::jdk_meta> faceDetectNode::handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) {
	if (!meta)
		return nullptr;
	std::cout << "[faceDetectNode] handle_control_meta: control_type = " << meta->control_type << std::endl;
	if (meta->control_type == jdk_objects::jdk_control_type::SPEAK)
		this->stop();

	return meta;
};

}  // namespace jdk_nodes
