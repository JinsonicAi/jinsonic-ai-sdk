#include "JdkNetServerNode.hpp"

#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <fstream>
#include <iostream>
#include <thread>

#include "post_node_info.h"

namespace jdk_nodes {
#define MAX_FRAME_SIZE (2 * 1024 * 1024)
static bool looks_annexb(const std::vector<uint8_t>& v) {
	for (size_t i = 0; i + 3 < v.size(); ++i)
		if (v[i] == 0 && v[i + 1] == 0 && (v[i + 2] == 1 || (i + 4 < v.size() && v[i + 2] == 0 && v[i + 3] == 1)))
			return true;
	return false;
}

NetServerNode::NetServerNode(std::string node_name, int device_id, int channel_id, bool rtsp_enable, int rtsp_port, std::string user, std::string pass, std::string task_id, size_t encode_queue_capacity)
	: device_id_(device_id), channel_id_(channel_id), rtsp_enable_(rtsp_enable), rtsp_port_(rtsp_port), user_(user), pass_(pass), task_id_(task_id), encode_queue_capacity_(std::max<size_t>(1, encode_queue_capacity)) {
	consumer_id_ = node_name;
	if (rtsp_enable_) {
		rtsp_ = std::make_shared<RTSPServer>("ch1", device_id_, rtsp_port_, VideoCodecType::H264, user_, pass_);
	}
	reporter_.set_output_rtsp_config({task_id_,
									  PLUGIN_NODE_NAME,
									  (rtsp_enable_ && rtsp_) ? rtsp_->rtsp_url() : "N/A",
									  "H264"});
	fmt::print("✅ NetServerNode constructed! BUILD TIME: {} {}\n", __DATE__, __TIME__);
}

NetServerNode::~NetServerNode() {
	stop();
	fmt::print("✅ NetServerNode destructed!\n");
}

void NetServerNode::stop() {
	std::lock_guard<std::mutex> lk(mutex_);
	set_alive(false);
	fmt::print("rtsp_ stop ...\n");
	if (rtsp_) {
		rtsp_->deinit();
		rtsp_.reset();
	}
	fmt::print("task encoder unsubscribe ...\n");
	unsubscribe_task_encoder(task_id_, consumer_id_);
	fmt::print("✅ NetServerNode stop ok!\n");
}
static uint32_t getTimestamp() {
	struct timeval tv = {0};
	gettimeofday(&tv, NULL);
	uint32_t ts = ((tv.tv_sec * 1000) + ((tv.tv_usec + 500) / 1000)) * 90;	// clockRate/1000;
	return ts;
}

// handle frame meta one by one
std::shared_ptr<jdk_objects::jdk_meta> NetServerNode::handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) {
	// fmt::print("NetServerNode handle_frame_meta, meta create_time: {}, frame pts: {}\n", meta->create_time, meta->frame_ ? meta->frame_->pts : -1);
	if (!meta || (!meta->frame_)) {
		std::cerr << "❌ meta is null or contains no valid frame.\n";
		return jdk_node_base::handle_frame_meta(meta);
	}
	std::shared_ptr<AXVideoFrame> frame =
		meta->dump_frame_ ? meta->dump_frame_  // use decorated frame if we have one
						  : meta->frame_;	   // otherwise fall back to raw frame

	if (!frame) {  // extra safety‑guard
		fprintf(stderr, "❌ NetServerNode received a nullptr frame, skipping\n");
		return jdk_node_base::handle_frame_meta(meta);
	}
	// Automatic timing, write the time spent to reporter_ at the end of the function.
	MetricsReporter::ScopedTimer timer(reporter_);
	// auto						 last_time = std::chrono::system_clock::now();
	std::lock_guard<std::mutex> lk(mutex_);
	if (!is_alive()) {
		fmt::print("NetServerNode is not alive, skipping frame_meta handling.\n");
		return nullptr;
	}

	////////////////////////
	TaskEncodeRequest req;
	req.device_id = device_id_;
	// Use task-assigned channel as encoder group to avoid cross-topology mismatch.
	req.group		   = channel_id_;
	req.channel		   = channel_id_;
	req.rtsp_port	   = rtsp_port_;
	req.queue_capacity = encode_queue_capacity_;
	auto encoded_frame = get_task_encoded_frame(task_id_, consumer_id_, frame, req);
	if (!encoded_frame) {
		fmt::print("get_task_encoded_frame failed, skipping frame_meta handling.\n");
		return jdk_node_base::handle_frame_meta(meta);
	}
	auto dump_frame = encoded_frame->toHost();
	if (!dump_frame || !dump_frame->getPviraddr()) {
		fprintf(stderr, "❌ dump_frame is nullptr or getPviraddr() failed!");
		return jdk_node_base::handle_frame_meta(meta);
	}
	size_t	 sz	  = dump_frame->size();
	uint8_t* data = reinterpret_cast<uint8_t*>(dump_frame->getPviraddr());
	if (sz > 0 && sz < MAX_FRAME_SIZE) {
		std::vector<uint8_t> owned(data, data + sz);
		if (rtsp_enable_ && rtsp_) {
			AX_U64 pts = frame->pts;
			rtsp_->send_nalu(owned.data(), (int)owned.size(), pts);
		}

		SdkFrame Frame{};
		Frame.size			 = owned.size();
		Frame.frameData		 = owned.data();
		Frame.presentationTs = getTimestamp();
		SdkWriteVideoFrame(task_id_.data(), &Frame);
	} else {
		fprintf(stderr, "❌ Encoded frame size %zu exceeds MAX_FRAME_SIZE %d, dropping frame!\n", sz, MAX_FRAME_SIZE);
	}

	std::string Resolution	   = std::to_string(frame->width()) + "x" + std::to_string(frame->height());
	const int	fps_for_report = std::max(0, jdk_node_base::node_fps());
	reporter_.report_output_rtsp(Resolution, fps_for_report, meta->create_time);
	// fmt::print("NetServerNode handle_frame_meta done, meta create_time: {}, frame pts: {}, resolution: {}, fps: {}\n",
	//            meta->create_time, frame->pts, Resolution, fps_for_report);
	return jdk_node_base::handle_frame_meta(meta);
}

void NetServerNode::handle_frame_meta(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& meta_with_batch) {
	const auto& frame_meta_with_batch = meta_with_batch;
	// run_infer_combinations(frame_meta_with_batch);
}

std::shared_ptr<jdk_objects::jdk_meta> NetServerNode::handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) {
	if (!meta)
		return nullptr;
	std::cout << "[NetServerNode] handle_control_meta: control_type = " << meta->control_type << std::endl;
	if (meta->control_type == jdk_objects::jdk_control_type::SPEAK) {
		stop();
		fmt::print("✅ NetServerNode stop ok!\n");
	}
	return meta;
}

}  // namespace jdk_nodes
