#include "JdkNetServerNode.hpp"

#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <fstream>
#include <iostream>
#include <thread>
#include <utility>

#include "PluginFrameUtils.hpp"
#include "post_node_info.h"

namespace jdk_nodes {
#define MAX_FRAME_SIZE (2 * 1024 * 1024)
NetServerNode::NetServerNode(std::string node_name, PluginRuntime runtime, int channel_id, bool rtsp_enable, int rtsp_port,
							 std::string user, std::string pass, std::string task_id,
							 size_t encode_queue_capacity)
	: device_id_(runtime.runtime_device_id),
	  channel_id_(channel_id),
	  rtsp_enable_(rtsp_enable),
	  rtsp_port_(rtsp_port),
	  user_(std::move(user)),
	  pass_(std::move(pass)),
	  task_id_(std::move(task_id)),
	  encode_queue_capacity_(std::max<size_t>(1, encode_queue_capacity)),
	  runtime_(std::move(runtime)) {
	consumer_id_ = node_name;
	if (rtsp_enable_) {
		rtsp_		= std::make_shared<RTSPServer>("ch1", device_id_, rtsp_port_, VideoCodecType::H264, user_, pass_);
		rtsp_ready_ = rtsp_ && rtsp_->is_ready();
		if (!rtsp_ready_) {
			if (rtsp_) {
				rtsp_->deinit();
				rtsp_.reset();
			}
			throw std::runtime_error(
				"RTSP server failed to listen on allocated port " + std::to_string(rtsp_port_));
		}
	}
	refresh_rtsp_urls(true);
	printf("NetServerNode constructed! task=%s node=%s rtsp_enable=%d rtsp_port=%d rtsp_url=%s BUILD TIME: %s %s\n",
		   task_id_.c_str(), consumer_id_.c_str(), rtsp_enable_, rtsp_port_, rtsp_url_.c_str(), __DATE__, __TIME__);
	fmt::print("✅ NetServerNode constructed! BUILD TIME: {} {}\n", __DATE__, __TIME__);
}

NetServerNode::~NetServerNode() {
	stop();
	fmt::print("✅ NetServerNode destructed!\n");
}

void NetServerNode::stop() {
	std::lock_guard<std::mutex> lk(mutex_);
	set_alive(false);
	unregister_task_rtsp_output(task_id_.c_str(), consumer_id_.c_str());
	fmt::print("rtsp_ stop ...\n");
	rtsp_ready_				= false;
	rtsp_send_error_logged_ = false;
	rtsp_url_.clear();
	rtsp_urls_.clear();
	if (rtsp_) {
		rtsp_->deinit();
		rtsp_.reset();
	}
	fmt::print("task encoder unsubscribe ...\n");
	unsubscribe_task_encoder(task_id_, consumer_id_);
	fmt::print("✅ NetServerNode stop ok!\n");
}

void NetServerNode::refresh_rtsp_urls(bool force) {
	const auto now = std::chrono::steady_clock::now();
	if (!force && last_rtsp_url_refresh_.time_since_epoch().count() != 0 &&
		now - last_rtsp_url_refresh_ < std::chrono::seconds(5)) {
		return;
	}
	last_rtsp_url_refresh_ = now;

	std::vector<std::string> urls;
	if (rtsp_enable_ && rtsp_ready_ && rtsp_ && rtsp_->is_ready())
		urls = rtsp_->rtsp_urls();
	const std::string primary = urls.empty() ? std::string() : urls.front();
	if (!force && urls == rtsp_urls_ && primary == rtsp_url_)
		return;

	rtsp_urls_ = std::move(urls);
	rtsp_url_  = primary;
	register_task_rtsp_output(task_id_.c_str(), consumer_id_.c_str(),
							  rtsp_url_.c_str(), "H264", rtsp_port_);
	const std::string urls_json = nlohmann::json(rtsp_urls_).dump();
	register_task_rtsp_output_urls(task_id_.c_str(), consumer_id_.c_str(),
								   urls_json.c_str());
	reporter_.set_output_rtsp_config({task_id_,
									  PLUGIN_NODE_NAME,
									  rtsp_url_.empty() ? "N/A" : rtsp_url_,
									  "H264"});
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
	refresh_rtsp_urls(false);

	////////////////////////
	TaskEncodeRequest req;
	req.device_id = device_id_;
	// Use task-assigned channel as encoder group to avoid cross-topology mismatch.
	req.group			 = channel_id_;
	req.channel			 = channel_id_;
	req.rtsp_port		 = rtsp_port_;
	req.queue_capacity	 = encode_queue_capacity_;
	req.runtime_location = runtime_.location;
	auto encoded_frame	 = get_task_encoded_frame(task_id_, consumer_id_, frame, req);
	if (!encoded_frame) {
		fmt::print("get_task_encoded_frame failed, skipping frame_meta handling.\n");
		return jdk_node_base::handle_frame_meta(meta);
	}
	const bool encoded_on_axcl = encoded_frame->backend() == VFrameBackend::Axcl ||
								 encoded_frame->memoryDomain() == VFrameMemoryDomain::AxclDevice;
	auto dump_frame = (!encoded_on_axcl && encoded_frame->cpuAccessible()) ? encoded_frame : encoded_frame->toHost();
	if (!jdk_plugin::frame_has_host_memory(dump_frame)) {
		fprintf(stderr, "❌ dump_frame is nullptr or getPviraddr() failed!");
		return jdk_node_base::handle_frame_meta(meta);
	}
	size_t	 sz	  = dump_frame->size();
	uint8_t* data = reinterpret_cast<uint8_t*>(dump_frame->getPviraddr());
	if (sz > 0 && sz < MAX_FRAME_SIZE) {
		if (rtsp_enable_ && rtsp_ready_ && rtsp_ && rtsp_->is_ready()) {
			AX_U64 pts = frame->pts;
			rtsp_->send_nalu(data, static_cast<int>(sz), pts);
			// if (int ret = rtsp_->send_nalu(data, static_cast<int>(sz), pts);0 != ret) {
			// 	printf("data:%p, size:%zu, pts:%llu, send_nalu failed![%d]\n", data, sz, pts, ret);
			// 	// rtsp_ready_ = false;
			// 	// rtsp_url_.clear();
			// 	// if (!rtsp_send_error_logged_) {
			// 	// 	rtsp_send_error_logged_ = true;
			// 		std::cerr << "[NetServerNode] RTSP send failed, disable RTSP push. task=" << task_id_
			// 				  << " node=" << consumer_id_ << " port=" << rtsp_port_ << std::endl;
			// 	// }
			// 	// unregister_task_rtsp_output(task_id_.c_str(), consumer_id_.c_str());
			// 	// register_task_rtsp_output(task_id_.c_str(), consumer_id_.c_str(), "", "H264", rtsp_port_);
			// 	// reporter_.set_output_rtsp_config({task_id_, PLUGIN_NODE_NAME, "N/A", "H264"});
			// 	// rtsp_->deinit();
			// 	// rtsp_.reset();
			// }
		}

		SdkFrame Frame{};
		Frame.size			 = sz;
		Frame.frameData		 = data;
		Frame.presentationTs = getTimestamp();
		// Each preview session is named after its task ID. Broadcasting here makes
		// every task's encoded frame enter every active preview channel.
		SdkWriteVideoFrame(task_id_.c_str(), &Frame);

		// SdkWriteVideoFrame(task_id_.c_str(), &Frame);
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
	return jdk_node_base::handle_control_meta(meta);
}

}  // namespace jdk_nodes
