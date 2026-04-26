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

int replace_task_decoder_channel(const std::string& runtime_location, int old_channel);

class NetClient {
public:
	NetClient(int device_id, int group, int channel, stream_info info, std::string task_name);
	~NetClient();

	bool						  start(const std::string &rtsp_url);
	std::shared_ptr<AXVideoFrame> get_frame(std::atomic<bool> *thread_alive);
	void						  on_frame(const EncodedAU &au);
	void						  stop();
	void                          set_runtime_location(std::string runtime_location) { runtime_location_ = std::move(runtime_location); }
	void                          reset_decoder_after_stall(uint64_t now_ms);

private:
	std::string rtsp_url_;

	bool hwcode_init_ = false;

	std::string				   task_name_{};
	std::string                runtime_location_{};
	int						   device_id_;
	int						   group_;
	int						   channel_id_{};
	std::shared_ptr<HwDecoder> decoder_ = nullptr;
	//
		stream_info								  info_{};
		std::unique_ptr<RtspPull>				  rtsp_{nullptr};
		std::atomic<bool>						  stop_requested_{false};  // exit signs
		std::atomic<bool>						  thread_alive_{false};
			std::atomic<uint64_t>					  next_decoder_init_retry_ms_{0};
			std::atomic<uint64_t>					  last_decoder_init_log_ms_{0};
			std::atomic<uint64_t>					  last_timeout_log_ms_{0};
			std::atomic<uint64_t>					  last_keyframe_wait_log_ms_{0};
			std::atomic<uint64_t>					  last_keyframe_resume_log_ms_{0};
			std::atomic<uint64_t>					  last_reset_keyframe_wait_log_ms_{0};
			std::atomic<bool>						  need_keyframe_after_reset_{false};
			std::atomic<uint32_t>					  decoder_init_failures_{0};
			std::atomic<uint32_t>                  decoder_rebind_failures_{0};
			safe_queue<std::shared_ptr<AXVideoFrame>> queue_;
			std::mutex								  decoder_mtx_;
		};

#endif	// __AX_NET_CLIENT_H__
