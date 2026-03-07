#ifndef __AX_NET_CLIENT_H__
#define __AX_NET_CLIENT_H__

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "HwDecoder.hpp"
#include "RtspClient.hpp"
#include "vdec_deque.hpp"

typedef unsigned char *LPBYTE;

class NetClient {
public:
	NetClient(int device_id, int group, int channel, stream_info info, std::string task_name);
	~NetClient();

	bool						  start(const std::string &rtsp_url);
	std::shared_ptr<AXVideoFrame> get_frame(std::atomic<bool> *thread_alive);
	void						  on_frame(const EncodedAU &au);
	void						  stop();

private:
	std::string rtsp_url_;

	bool hwcode_init_ = false;

	std::string				   task_name_{};
	int						   device_id_;
	int						   group_;
	int						   channel_id_{};
	std::shared_ptr<HwDecoder> decoder_ = nullptr;
	//
	stream_info								  info_{};
	std::unique_ptr<RtspPull>				  rtsp_{nullptr};
	std::atomic<bool>						  stop_requested_{false};  // exit signs
	std::atomic<bool>						  thread_alive_{false};
	safe_queue<std::shared_ptr<AXVideoFrame>> queue_;
	std::mutex								  decoder_mtx_;
};

#endif	// __AX_NET_CLIENT_H__
