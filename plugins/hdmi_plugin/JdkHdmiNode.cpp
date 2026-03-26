#include "JdkHdmiNode.hpp"

#include "hdmi.h"
#include "post_node_info.h"

namespace jdk_nodes {
HdmiNode::HdmiNode(std::string node_name, int device_id, int hdmi_device_id, std::string task_id)
	: device_id_(device_id), hdmi_device_id_(hdmi_device_id), task_id_(task_id) {
	reporter_.set_algorithm_config({task_id_,
									PLUGIN_NODE_NAME,
									"-"});
	fmt::print("✅ HdmiNode constructed! BUILD TIME: {} {}\n", __DATE__, __TIME__);
}

HdmiNode::~HdmiNode() {
	stop();
	fmt::print("✅ HdmiNode destructed!\n");
}

void HdmiNode::stop() {
	set_alive(false);
	std::lock_guard<std::mutex> lk(mutex_);

	fmt::print("✅ HdmiNode stop ok!\n");
}

void frame_info(AX_VIDEO_FRAME_T& frame) {
	fmt::print(" [hdmi] frame->width:{}, frame->height:{}, frame->Stride:[{},{},{}],enImgFormat:{} \r\n", frame.u32Width, frame.u32Height, frame.u32PicStride[0], frame.u32PicStride[1], frame.u32PicStride[2], frame.enImgFormat);
}
// handle frame meta one by one
std::shared_ptr<jdk_objects::jdk_meta> HdmiNode::handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) {
	std::lock_guard<std::mutex> lk(mutex_);
	if (!is_alive()) return nullptr;
	if (!meta || (!meta->frame_)) {
		std::cerr << "❌ meta is null or contains no valid frame.\n";
		return jdk_node_base::handle_frame_meta(meta);
	}
	std::shared_ptr<AXVideoFrame> canvas =
		meta->dump_frame_ ? meta->dump_frame_  // use decorated frame if we have one
						  : meta->frame_;	   // otherwise fall back to raw frame
	// Automatic timing, write the time spent to reporter_ at the end of the function.
	MetricsReporter::ScopedTimer timer(reporter_);

	if (!canvas) {	// extra safety‑guard
		fprintf(stderr, "❌ NetServerNode received a nullptr frame, skipping\n");
		return jdk_node_base::handle_frame_meta(meta);
	}
	auto last_time = std::chrono::system_clock::now();

	auto start_time = std::chrono::steady_clock::now();
	auto hostframe	= canvas->toHost();
	HdmiSendFrame(hdmi_device_id_, task_id_, *hostframe->raw());

	// report node info
	reporter_.report_algorithm(jdk_node_base::node_fps(), meta->create_time);
	// for fps
	auto snap = std::chrono::system_clock::now() - last_time;
	snap	  = std::chrono::duration_cast<std::chrono::milliseconds>(snap);
	if (std::chrono::milliseconds delta = std::chrono::milliseconds(1000 / 30); snap < delta) {
		std::this_thread::sleep_for(delta - snap);
	}
	return jdk_node_base::handle_frame_meta(meta);
}

void HdmiNode::handle_frame_meta(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& meta_with_batch) {
	const auto& frame_meta_with_batch = meta_with_batch;
	// run_infer_combinations(frame_meta_with_batch);
}

std::shared_ptr<jdk_objects::jdk_meta> HdmiNode::handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) {
	if (!meta) return nullptr;
	std::cout << "[HdmiNode] handle_control_meta: control_type = " << meta->control_type << std::endl;
	if (meta->control_type == jdk_objects::jdk_control_type::SPEAK) this->stop();

	return meta;
};

}  // namespace jdk_nodes