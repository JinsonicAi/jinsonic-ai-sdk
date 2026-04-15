#ifndef __NET_CLIENT_JDK_NODE_HPP__
#define __NET_CLIENT_JDK_NODE_HPP__

#include <filesystem>
#include <string>
#include <thread>

#include "jdk_node_base.hpp"
#include "jdk_node_wrapper.hpp"
//
#include "HwEncoder.hpp"

//
#include "MetricsReporter.hpp"
#include "Task.hpp"

namespace jdk_nodes {
class NetServerNode : public jdk_node_base, public CustomHandleFrame, public CustomHandleControl {
public:
	NetServerNode(std::string node_name, int device_id, int channel_id, bool rtsp_enable, int rtsp_port, std::string user, std::string pass, std::string task_id = "", size_t encode_queue_capacity = 3);
	~NetServerNode();
	void stop();

protected:
	// re-implementation for one by one mode, marked as 'final' as we need not override any more in specific derived classes.
	virtual std::shared_ptr<jdk_objects::jdk_meta> handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) override final;
	// re-implementation for batch by batch mode, marked as 'final' as we need not override any more in specific derived classes.
	virtual void						   handle_frame_meta(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& meta_with_batch) override final;
	std::shared_ptr<jdk_objects::jdk_meta> handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) override;

private:
	std::string task_id_{};
	size_t		encode_queue_capacity_{3};
	int			rtsp_port_		  = -1;
	int			device_id_		  = -1;
	int			channel_id_		  = 0;	//< 32
	int			channel_mjpeg_id_ = 0;	//< 32
	std::string rtsp_url_;
	//
	std::string consumer_id_{};
	// std::shared_ptr<RtspServer> rtsp_{nullptr};
	std::shared_ptr<RTSPServer> rtsp_{nullptr};
	bool						rtsp_enable_{false};  //< enable rtsp push
	// std::vector<uint8_t>		frame_tmp;
	std::string		user_;
	std::string		pass_;
	std::mutex		mutex_;
	MetricsReporter reporter_{5};
};
}  // namespace jdk_nodes

#endif
