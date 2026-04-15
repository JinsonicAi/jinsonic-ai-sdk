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
	int			device_id_;	  //-1 host,0 >= device
	int			group_;		  // < 32
	int			channel_id_;  // default 0

	std::string				   rtsp_url_;
	bool					   rtsp_init_	 = false;
	// 关键：避免在持 mutex_ 时调用 NetClient 的潜在阻塞接口（get_frame/start/stop）
	// 使用 shared_ptr 便于在锁外安全拷贝引用，消除 stop() 与 handle_run() 之间的互锁死锁。
	std::shared_ptr<NetClient> net_client	 = {nullptr};
	int						   skip_interval = 0;
	stream_info				   info_{};
	//
	std::mutex		mutex_;
	MetricsReporter reporter_{5};

	// ---- 按时布控 (schedule-based pause/resume) ----
	nlohmann::json		schedule_config_;			 // 布控配置 JSON
	std::atomic<bool>	schedule_paused_{false};		 // 当前是否被布控暂停
	std::atomic<bool>	schedule_running_{false};	 // checker 线程运行标志
	std::atomic<bool>	schedule_joined_{false};		 // 保证 join 只执行一次
	std::thread			schedule_thread_;			 // 定时检查线程

	void start_schedule_checker();					 // 启动定时布控检查线程
	void stop_schedule_checker();					 // 停止定时布控检查线程
	bool is_in_schedule_now() const;				 // 判断当前时间是否在布控时段内
};
}  // namespace jdk_nodes

#endif