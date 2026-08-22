#ifndef __NET_CLIENT_JDK_NODE_HPP__
#define __NET_CLIENT_JDK_NODE_HPP__

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "jdk_node_base.hpp"
#include "jdk_node_wrapper.hpp"
#include "PluginRuntime.hpp"
//
#include "HwEncoder.hpp"

//
#include "MetricsReporter.hpp"
#include "Task.hpp"

namespace jdk_nodes {
class NetServerNode : public jdk_node_base, public CustomHandleFrame, public CustomHandleControl {
public:
	NetServerNode(std::string node_name, PluginRuntime runtime, int channel_id, bool rtsp_enable, int rtsp_port,
				  std::string user, std::string pass, std::string task_id = "",
				  size_t encode_queue_capacity = 3);
	~NetServerNode();
	void stop();

protected:
	// re-implementation for one by one mode, marked as 'final' as we need not override any more in specific derived classes.
	virtual std::shared_ptr<jdk_objects::jdk_meta> handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) override final;
	// re-implementation for batch by batch mode, marked as 'final' as we need not override any more in specific derived classes.
	virtual void						   handle_frame_meta(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& meta_with_batch) override final;
	std::shared_ptr<jdk_objects::jdk_meta> handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) override;

private:
	struct PendingEncodeFrame {
		std::shared_ptr<AXVideoFrame> frame;
	};

	struct PendingOutputFrame {
		std::shared_ptr<AXVideoFrame> frame;
		AX_U64					  pts{0};
	};

	void refresh_rtsp_urls(bool force);
	void start_encode_worker();
	void stop_encode_worker();
	void enqueue_encode(std::shared_ptr<AXVideoFrame> frame);
	void encode_loop();
	void drain_encoded_output(std::shared_ptr<AXVideoFrame> encoded_frame);
	void start_output_worker();
	void stop_output_worker();
	void enqueue_output(std::shared_ptr<AXVideoFrame> frame, AX_U64 pts);
	void output_loop();

	std::string task_id_{};
	size_t		encode_queue_capacity_{3};
	PluginRuntime runtime_{};
	int			rtsp_port_		  = -1;
	int			device_id_		  = -1;
	int			channel_id_		  = 0;	//< 32
	int			channel_mjpeg_id_ = 0;	//< 32
	std::string rtsp_url_;
	std::vector<std::string> rtsp_urls_;
	std::chrono::steady_clock::time_point last_rtsp_url_refresh_{};
	//
	std::string consumer_id_{};
	// std::shared_ptr<RtspServer> rtsp_{nullptr};
	std::shared_ptr<RTSPServer> rtsp_{nullptr};
	bool						rtsp_enable_{false};  //< enable rtsp push
	bool						rtsp_ready_{false};
	bool						rtsp_send_error_logged_{false};
	// std::vector<uint8_t>		frame_tmp;
	std::string		user_;
	std::string		pass_;
	std::mutex		mutex_;
	std::mutex		encode_mutex_;
	std::condition_variable encode_cv_;
	std::deque<PendingEncodeFrame> encode_input_queue_;
	std::thread		encode_thread_;
	std::atomic<bool> encode_running_{false};
	size_t			encode_input_capacity_{1};
	std::atomic<uint64_t> encode_input_dropped_{0};
	std::mutex		output_mutex_;
	std::condition_variable output_cv_;
	std::condition_variable output_space_cv_;
	std::deque<PendingOutputFrame> output_queue_;
	std::thread		output_thread_;
	std::atomic<bool> output_running_{false};
	size_t			output_queue_capacity_{6};
	std::atomic<uint64_t> output_enqueued_{0};
	std::atomic<uint64_t> output_sent_{0};
	std::atomic<uint64_t> output_dropped_{0};
	std::atomic<uint64_t> output_send_failed_{0};
	// Five-second delivery rate measured at the actual RTSP/cloud sender.  The
	// graph worker's legacy node_fps uses a 500 ms scheduling window and can read
	// 9 FPS while this asynchronous sender is continuously delivering 25-30 FPS.
	// Export the sink rate so health checks reflect user-visible throughput.
	std::atomic<uint32_t> output_delivery_fps_{0};
	MetricsReporter reporter_{5};
};
}  // namespace jdk_nodes

#endif
