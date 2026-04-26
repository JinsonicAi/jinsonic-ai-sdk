#ifndef __NET_CLIENT_JDK_NODE_HPP__
#define __NET_CLIENT_JDK_NODE_HPP__

#include <atomic>
#include <deque>
#include <filesystem>
#include <string>
#include <thread>

#include "jdk_node_base.hpp"
#include "jdk_node_wrapper.hpp"
//
#include "MetricsReporter.hpp"
#include "NetClient.hpp"
//
#include "json.hpp"

namespace jdk_nodes {
class NetClientNode : public jdk_node_base, public CustomHandleRun, public CustomHandleControl {
public:
	NetClientNode(std::string node_name, std::string rtsp_url, int device_id, int group, int channel,
				  stream_info info, std::string task_id = "", std::string task_name = "",
				  nlohmann::json schedule_config = nlohmann::json{});
	~NetClientNode();
	void stop();

protected:
	virtual void handle_run(std::stop_token stoken) override;
	// bool		 has_custom_handle_run() const override { return true; }
	virtual std::shared_ptr<jdk_objects::jdk_meta> handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) override;

	int frame_index = {0};

private:
	std::shared_ptr<jdk_objects::jdk_frame_meta> build_frame_meta(
		const std::shared_ptr<AXVideoFrame>& frame,
		int									 frame_index,
		int									 channel_id,
		int									 fps);
	void		report_rtsp_info(const std::shared_ptr<AXVideoFrame>& frame, int fps);
	std::string task_id_{};
	std::string task_name_{};
	int		device_id_;	  //-1 host, 0 >= device
	int			group_;		  // < 32
	int			channel_id_;  // default 0

	std::string				   rtsp_url_;
	bool					   rtsp_init_	 = false;
	// key: avoid blocking NetClient methods (get_frame/start/stop) while holding mutex_
	// use shared_ptr to safely copy ref outside lock, eliminating deadlock between stop() and handle_run()
	std::shared_ptr<NetClient> net_client	 = {nullptr};
	int						   skip_interval = 0;
	stream_info				   info_{};
	//
	std::mutex		mutex_;
	MetricsReporter reporter_{5};

	// ---- time-based control (schedule-based pause/resume) ----
	nlohmann::json		schedule_config_;		 // schedule config JSON
	std::atomic<bool>	schedule_paused_{false};		 // whether currently paused by schedule
	std::atomic<bool>	schedule_running_{false};	 // checker thread running flag
	std::atomic<bool>	schedule_joined_{false};		 // ensure join only executes once
	std::thread			schedule_thread_;		 // periodic check thread

	void start_schedule_checker();					 // start periodic schedule check thread
	void stop_schedule_checker();					 // stop periodic schedule check thread
	bool is_in_schedule_now() const;				 // check if current time is in scheduled period
};
}  // namespace jdk_nodes

#endif