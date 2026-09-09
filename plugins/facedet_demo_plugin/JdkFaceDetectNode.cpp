#include "JdkFaceDetectNode.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include "DevProtoDef.hpp"
#include "DeviceInfo.hpp"
#include "HwIvps.hpp"
#include "JdkOsd.hpp"
#include "alg_comm.hpp"
#include "anyx.hpp"
#include "post_node_info.h"

namespace jdk_nodes {
namespace {
int64_t get_current_timestamp() {
	auto now = std::chrono::system_clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}
}  // namespace

faceDetectV2Node::faceDetectV2Node(std::string		  node_name,
								   const std::string& filename,
								   float			  threshold,
								   PluginRuntime	  runtime,
								   std::string		  task_id,
								   int				  label_score_step,
								   int				  confirm_frames,
								   float			  track_iou,
								   int				  track_max_missed,
								   int				  alarm_push_interval_ms)
	: channel_id_(runtime.runtime_device_id),
	  threshold_(threshold),
	  runtime_(std::move(runtime)),
	  device_id_(runtime_.runtime_device_id),
	  label_score_step_(std::clamp(label_score_step, 0, 100)),
	  task_id_(task_id),
	  confirm_frames_(std::clamp(confirm_frames, 1, 120)),
	  track_iou_(std::clamp(track_iou, 0.05f, 0.95f)),
	  track_max_missed_(std::clamp(track_max_missed, 1, 600)),
	  alarm_push_interval_ms_(std::clamp(alarm_push_interval_ms, 1000, 600000)) {
	infer = YOLOV5FACE::create_infer(filename, runtime_.infer_type(), device_id_);
	if (!infer) {
		throw std::runtime_error(fmt::format("face detector failed to create {} backend for runtime_location={} device_id={}",
			runtime_.infer_type(), runtime_.location, device_id_));
	}
	YOLOV5FACE::Params infer_params;
	infer->paramsGet(infer_params);
	infer_params.confThreshold_ = threshold_;
	infer->paramsSet(infer_params);
	fmt::print("[FaceDet] runtime_location={} infer_type={} runtime_device_id={}\n",
		runtime_.location, runtime_.infer_type(), device_id_);
	fmt::print("[FaceDet] detection_threshold={} confirm_frames={} track_iou={} track_max_missed={} alarm_push_interval_ms={}\n",
		threshold_, confirm_frames_, track_iou_, track_max_missed_, alarm_push_interval_ms_);
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

	YOLOV5FACE::Objects det;
	bool				ok = anyx::visit<YOLOV5FACE::Objects>(payload_any, [&](const auto& v) { det = v; });

	if (ivps) {
		for (const auto& b : det) {
			if (b.prob < threshold_)
				continue;
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

jdk_osd::Overlay faceDetectV2Node::build_overlay_(const YOLOV5FACE::Objects& det, int frame_w, int frame_h) {
	jdk_osd::Overlay overlay;
	const int max_dim = std::max(frame_w, frame_h);
	const int line_thickness = std::clamp((max_dim + 639) / 640 + 1, 3, 10);
	const int label_font_size = std::clamp(static_cast<int>(std::lround(max_dim * 0.018f)), 18, 42);
	const int keypoint_radius = std::max(2, line_thickness + 1);

	overlay.boxes.reserve(det.size());
	for (const auto& b : det) {
		if (b.prob < threshold_)
			continue;
		if (b.rect.width * b.rect.height <= 200)
			continue;
		jdk_osd::Box box;
		bool		 is_ghost	  = false;
		box.x					  = b.rect.x;
		box.y					  = b.rect.y;
		box.w					  = b.rect.width;
		box.h					  = b.rect.height;
		box.score				  = b.prob;
		box.track_id			  = b.track_id;
		box.ghost				  = is_ghost;
		box.priority			  = is_ghost ? 10 : 100;
		const AX_U32 color_rgb	  = random_color(box.track_id ? static_cast<int>(box.track_id) : b.label);
		box.style.color			  = is_ghost ? jdk_osd::Color::from_rgb(color_rgb, 180)
											 : jdk_osd::Color::from_rgb(color_rgb, 255);
		box.style.thickness		  = line_thickness;
		box.style.alpha			  = box.style.color.a;
		box.label_style.font_size = label_font_size;
		box.label_style.fg		  = jdk_osd::Color{255, 255, 255, 255};
		box.label_style.bg		  = box.style.color;
		box.label_style.bg_alpha  = box.style.color.a;
		int score_pct			  = static_cast<int>(std::round(std::clamp(b.prob, 0.0f, 1.0f) * 100.0f));
		if (label_score_step_ > 1) {
			score_pct = ((score_pct + label_score_step_ / 2) / label_score_step_) * label_score_step_;
			score_pct = std::clamp(score_pct, 0, 100);
		}
		box.label = fmt::format("face {}%", score_pct);
		overlay.boxes.push_back(std::move(box));
	}

	for (const auto& face : det) {
		if (face.prob < threshold_)
			continue;
		for (const auto& pt : face.landmarks) {
			jdk_osd::Keypoint kp;
			kp.p			   = {pt.x, pt.y};
			kp.radius		   = keypoint_radius;
			kp.style.color	   = jdk_osd::Color{0, 210, 255, 255};
			kp.style.thickness = 1;
			kp.priority		   = 80;
			overlay.keypoints.push_back(kp);
		}
	}
	return overlay;
}

namespace {
float rect_iou(const cv::Rect_<float>& a, const cv::Rect_<float>& b) {
	const float intersection = (a & b).area();
	const float union_area = a.area() + b.area() - intersection;
	return union_area > 0.0f ? intersection / union_area : 0.0f;
}

float rect_match_score(const cv::Rect_<float>& current, const cv::Rect_<float>& previous,
					   float min_iou) {
	const float iou = rect_iou(current, previous);
	if (iou >= min_iou) return 1.0f + iou;
	const cv::Point2f a(current.x + current.width * 0.5f,
						current.y + current.height * 0.5f);
	const cv::Point2f b(previous.x + previous.width * 0.5f,
						previous.y + previous.height * 0.5f);
	const float dx = a.x - b.x;
	const float dy = a.y - b.y;
	const float diag = std::sqrt(std::max(1.0f,
		previous.width * previous.width + previous.height * previous.height));
	const float distance_ratio = std::sqrt(dx * dx + dy * dy) / diag;
	const float area_a = std::max(1.0f, current.area());
	const float area_b = std::max(1.0f, previous.area());
	const float size_ratio = std::min(area_a, area_b) / std::max(area_a, area_b);
	if (iou >= 0.08f && distance_ratio <= 0.75f && size_ratio >= 0.35f) {
		return iou + (1.0f - distance_ratio) * 0.25f + size_ratio * 0.15f;
	}
	return 0.0f;
}
}  // namespace

void faceDetectV2Node::update_tracks_(YOLOV5FACE::Objects& det) {
	for (auto& [_, track] : tracks_)
		++track.missed_frames;

	struct MatchCandidate {
		float score{0.0f};
		uint64_t track_id{0};
		size_t face_index{0};
	};
	std::vector<MatchCandidate> candidates;
	for (const auto& [track_id, track] : tracks_) {
		if (track.missed_frames > track_max_missed_) continue;
		const float horizon = static_cast<float>(std::clamp(track.missed_frames, 1, 3));
		const cv::Rect_<float> predicted(
			track.bbox.x + track.velocity_x * horizon,
			track.bbox.y + track.velocity_y * horizon,
			std::max(1.0f, track.bbox.width + track.velocity_w * horizon),
			std::max(1.0f, track.bbox.height + track.velocity_h * horizon));
		for (size_t face_index = 0; face_index < det.size(); ++face_index) {
			const auto& face = det[face_index];
			if (face.prob < threshold_ || face.rect.area() <= 200.0f) continue;
			const float score = std::max(
				rect_match_score(face.rect, predicted, track_iou_),
				rect_match_score(face.rect, track.bbox, track_iou_));
			if (score > 0.30f) candidates.push_back({score, track_id, face_index});
		}
	}
	std::stable_sort(candidates.begin(), candidates.end(),
		[](const MatchCandidate& lhs, const MatchCandidate& rhs) {
			return lhs.score > rhs.score;
		});
	std::unordered_map<size_t, uint64_t> assignments;
	std::unordered_set<uint64_t> matched_tracks;
	std::unordered_set<size_t> matched_faces;
	for (const auto& candidate : candidates) {
		if (matched_tracks.count(candidate.track_id) || matched_faces.count(candidate.face_index))
			continue;
		matched_tracks.insert(candidate.track_id);
		matched_faces.insert(candidate.face_index);
		assignments.emplace(candidate.face_index, candidate.track_id);
	}

	const int64_t now_ms = get_current_timestamp();
	for (size_t face_index = 0; face_index < det.size(); ++face_index) {
		auto& face = det[face_index];
		if (face.prob < threshold_ || face.rect.area() <= 200.0f)
			continue;

		const auto assigned = assignments.find(face_index);
		uint64_t best_id = assigned == assignments.end() ? 0 : assigned->second;
		if (best_id == 0) {
			best_id = next_track_id_++;
			TrackState fresh;
			fresh.id = best_id;
			fresh.bbox = face.rect;
			fresh.first_seen_ms = now_ms;
			tracks_.emplace(best_id, fresh);
		}
		auto& track = tracks_.at(best_id);
		const auto previous = track.bbox;
		const float gap = static_cast<float>(std::max(1, track.missed_frames));
		constexpr float alpha = 0.40f;
		track.velocity_x = track.velocity_x * (1.0f - alpha) +
			((face.rect.x - previous.x) / gap) * alpha;
		track.velocity_y = track.velocity_y * (1.0f - alpha) +
			((face.rect.y - previous.y) / gap) * alpha;
		track.velocity_w = track.velocity_w * (1.0f - alpha) +
			((face.rect.width - previous.width) / gap) * alpha;
		track.velocity_h = track.velocity_h * (1.0f - alpha) +
			((face.rect.height - previous.height) / gap) * alpha;
		track.bbox = face.rect;
		++track.hits;
		track.missed_frames = 0;
		matched_tracks.insert(best_id);
		face.track_id = best_id;

		if (track.hits >= confirm_frames_ && !track.reported && !pending_alarms_.count(best_id)) {
			PendingAlarm pending;
			pending.track_id = best_id;
			pending.event_id = fmt::format("{}:face_detection:{}:{}", task_id_, best_id, track.first_seen_ms);
			pending.face = face;
			pending.created_at_ms = now_ms;
			pending_alarms_.emplace(best_id, std::move(pending));
		}
	}

	for (auto it = tracks_.begin(); it != tracks_.end();) {
		if (it->second.missed_frames > track_max_missed_) {
			it = tracks_.erase(it);
		} else {
			++it;
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

static std::string get_guid() {
	auto& dev = DeviceInfo::instance();
	return dev.deviceId();
}

std::pair<nlohmann::json, std::vector<std::function<std::shared_ptr<AXVideoFrame>()>>>
faceDetectV2Node::alarm_fn(const std::any& result_any, std::shared_ptr<AXVideoFrame> canvas) {
	(void)canvas;
	(void)result_any;
	std::vector<PendingAlarm> pending;
	{
		std::lock_guard<std::mutex> lk(mutex_);
		for (const auto& [track_id, alarm] : pending_alarms_) {
			pending.push_back(alarm);
			auto track_it = tracks_.find(track_id);
			if (track_it != tracks_.end())
				track_it->second.reported = true;
		}
		pending_alarms_.clear();
	}
	if (pending.empty())
		return {{}, {}};

	constexpr const char* kAlarmType = "faceAlarm";
	const int64_t		  now_ms	 = get_current_timestamp();
	const std::string	  now_iso	 = get_current_iso_time();

	auto to_bbox = [](const YOLOV5FACE::FaceBox& box) {
		return nlohmann::json{
			{"x", box.rect.x},
			{"y", box.rect.y},
			{"w", box.rect.width},
			{"h", box.rect.height}};
	};

	auto to_landmarks = [](const YOLOV5FACE::FaceBox& box) {
		nlohmann::json arr = nlohmann::json::array();
		for (const auto& pt : box.landmarks) {
			arr.push_back({{"x", pt.x}, {"y", pt.y}});
		}
		return arr;
	};

	auto build_alarm_item = [&](const PendingAlarm& pending_alarm) -> nlohmann::json {
		const auto& box = pending_alarm.face;
		const auto bbox		 = to_bbox(box);
		const auto landmarks = to_landmarks(box);

		nlohmann::json one = {
			{"local_push_msg", {{"alarm_type", kAlarmType}, {"event_id", pending_alarm.event_id}, {"track_id", pending_alarm.track_id}, {"confidence", box.prob}, {"label", box.label}, {"bbox", bbox}, {"landmarks", landmarks}}},
			{"alarm_push_msg", {{"did", get_guid()}, {"type", static_cast<int>(EventType::HUMAN_DETECTION)}, {"time", now_ms}, {"state", 0}, {"data", {{"event_id", pending_alarm.event_id}, {"event_type", "face_detection"}, {"track_id", pending_alarm.track_id}, {"confidence", box.prob}, {"label", box.label}, {"bbox", bbox}, {"landmarks", landmarks}}}}}};
		return one;
	};

	nlohmann::json alarm_arr = nlohmann::json::array();
	for (const auto& alarm : pending)
		alarm_arr.push_back(build_alarm_item(alarm));
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
	auto  result_sp	 = anyx::any_try_cast<YOLOV5FACE::Objects>(result_any);
	if (!result_sp) {
		fmt::print("⚠️ FaceDet skip: got {}\n", anyx::type_name(result_any));
		return;
	}
	YOLOV5FACE::Objects det;
	anyx::visit<YOLOV5FACE::Objects>(*result_sp, [&](const auto& v) { det = v; });
	update_tracks_(det);
	const bool has_face_event = !pending_alarms_.empty();
	auto overlay = build_overlay_(det,
		static_cast<int>(frame_meta->frame_->width()),
		static_cast<int>(frame_meta->frame_->height()));
	auto overlay_result = jdk_osd::make_overlay_result(
		std::make_shared<std::any>(std::move(det)),
		std::move(overlay));
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
			true, alarm_push_interval_ms_,
			has_face_event ? jdk_objects::AlarmDeliveryMode::ImmediateEvent
			               : jdk_objects::AlarmDeliveryMode::PeriodicState);
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

	return jdk_node_base::handle_control_meta(meta);
};

}  // namespace jdk_nodes
