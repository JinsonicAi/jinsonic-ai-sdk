#include "JdkCatDogDetectNode.hpp"

#include <iomanip>

#include "HwIvps.hpp"
#include "alg_comm.hpp"
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

/* -------------------------------- ctor / dtor ----------------------------- */
catDogDetectNode::catDogDetectNode(std::string		  node_name,
								   const std::string &filename,
								   float			  threshold,
								   int				  device_id,
								   std::string		  task_id)
	: channel_id_(device_id),
	  threshold_(threshold),
	  device_id_(device_id),
	  task_id_(std::move(task_id)) {
	fmt::print("#node_name:{}, filename:{}, device_id:{}, task_id:{}\n",
			   node_name, filename, device_id, task_id_);

	SafeAlgorithm::Options opt{ax_model_type_cat_dog, filename, device_id_};
	if (alg_ = std::make_shared<SafeAlgorithm>(opt); !alg_ || !alg_->ok()) {
		fmt::print(fg(fmt::color::red), "❌ catDogDetectNode algo init failed!\n");
		if (!alg_->ok()) {
			alg_->close();
			alg_.reset();
		}
		fmt::print("⚠️ catDogDetectNode constructed! BUILD TIME: {} {}\n", __DATE__, __TIME__);
		return;
	}
	alg_->set_affinity(true);
	alg_->update_params([&](auto &p) {
		p.det_threshold = threshold_;
	});
	reporter_.set_algorithm_config({task_id_,
									PLUGIN_NODE_NAME,
									std::to_string(threshold_)});
	Capture_ = std::make_shared<HwCapture>(device_id_);
	fmt::print("✅ catDogDetectNode constructed! BUILD TIME: {} {}\n", __DATE__, __TIME__);
}

catDogDetectNode::~catDogDetectNode() { stop(); }

void catDogDetectNode::stop() {
	set_alive(false);
	std::lock_guard<std::mutex> lk(mutex_);
	if (alg_) {
		alg_->close();
		alg_.reset();
	}
	if (Capture_) Capture_.reset();
	fmt::print("✅ catDogDetectNode stop ok!\n");
}

void catDogDetectNode::render_fn(std::shared_ptr<AXVideoFrame> &canvas,
								 const std::any				   &result_any,
								 const std::any				   &extra) {
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
catDogDetectNode::alarm_fn(const std::any &result_any,
						   std::shared_ptr<AXVideoFrame>) {
	ax_result_t value{};
	bool		ok = anyx::visit<ax_result_t>(result_any, [&](const auto &v) { value = v; });
	if (!ok) {
		if (!ok) fmt::print("⚠️ [{} Alarm] unexpected any type: {} \r\n", PLUGIN_NODE_NAME, anyx::type_name(result_any));
		return {{}, {}};
	}

	auto to_json = [](const auto &box) {
		return nlohmann::json{
			{"alarm_type", "cat_dog"},
			{"confidence", box.score},
			{"label", box.label},
			{"bbox",
			 {{"x", box.bbox.x}, {"y", box.bbox.y}, {"w", box.bbox.w}, {"h", box.bbox.h}}}};
	};

	auto build_root = [&](const ax_result_t &rst) {
		nlohmann::json arr = nlohmann::json::array();
		for (int i = 0; i < rst.n_objects; ++i) arr.push_back(to_json(rst.objects[i]));
		return nlohmann::json{
			{"msg", "TaskId:" + task_id_ + " " + jdk_node_base::node_name() +
						" AlarmType: catDogAlarm Timestamp:" + get_current_iso_time()},
			{"alarm_type", "catDogAlarm"},
			{"alarm_count", arr.size()},
			{"timestamp", get_current_iso_time()},
			{"alarms", arr}};
	};

	return {build_root(value), {}};

	// try {
	// 	if (future_any.type() == typeid(ax_result_t))
	// 		return {build_root(std::any_cast<const ax_result_t &>(future_any)), {}};
	// 	else {
	// 		auto sp = std::any_cast<std::shared_ptr<ax_result_t>>(future_any);
	// 		if (sp) return {build_root(*sp), {}};
	// 	}
	// } catch (const std::bad_any_cast &e) {
	// 	fprintf(stderr, "❌ alarm_fn any_cast failed: %s\n", e.what());
	// }
	// return {{}, {}};
}

void catDogDetectNode::run_infer_combinations(
	const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>> &batch) {
	assert(batch.size() == 1);
	auto &meta	= batch[0];
	auto  frame = meta->frame_;

	ax_result_t det{};
	// memset(&det, 0, sizeof(ax_result_t));
	// auto hostframe = frame->toHost();
	// auto alg_image = frame2image(hostframe, ax_color_space_nv12);
	// ax_algorithm_track(handle_, &alg_image, &det);
	if (alg_) alg_->track(frame, det, Capture_);

	auto					   det_sp = std::make_shared<ax_result_t>(det);
	std::weak_ptr<ax_result_t> det_wp = det_sp;
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
			   [this, det_wp](const std::any &) {
				   if (auto sp = det_wp.lock(); sp && sp->n_objects > 0)
					   return alarm_fn(sp, nullptr);
				   return std::make_pair(nlohmann::json::object(),
															 std::vector<std::function<std::shared_ptr<AXVideoFrame>()>>{});
			   },
			   /* push_enabled */ true,
			   /* push_interval_ms */ 50000);
		meta->result_map_[jdk_node_base::node_name()].exchange(new_entry);
	}
}

std::shared_ptr<jdk_objects::jdk_meta>
catDogDetectNode::handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) {
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

void catDogDetectNode::handle_frame_meta(
	const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>> &batch) {
	run_infer_combinations(batch);
}

std::shared_ptr<jdk_objects::jdk_meta> catDogDetectNode::handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) {
	if (!meta) return nullptr;
	std::cout << "[catDogDetectNode::] handle_control_meta: control_type = " << meta->control_type << std::endl;

	if (meta->control_type == jdk_objects::jdk_control_type::SPEAK) {
		this->stop();
	}
	return meta;
};

}  // namespace jdk_nodes