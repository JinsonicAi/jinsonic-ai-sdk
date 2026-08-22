#ifndef __RTSP_SERVER_HANDLE_H__
#define __RTSP_SERVER_HANDLE_H__
#include <atomic>
#include <cstdio>
#include <cstring>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "context.h"

// #include "rtsp_server_handle.h"

enum VideoCodecType {
	H264 = 96,
	H265 = 265,
	NONE,
};

class EXPORT_VISIBILITY RTSPServer {
public:
	RTSPServer(const std::string &stream_name, int stream_id = 0, int rtsp_port = 8554, VideoCodecType codec = H264,
			   std::string user = "admin", std::string password = "123456", const std::vector<uint8_t> &sps = {}, const std::vector<uint8_t> &pps = {}, const std::vector<uint8_t> &vps = {});
	~RTSPServer();

	// initialize the rtsp server specifying h264 h265
	bool init();
	// stop the rtsp server
	void deinit();

	// send h264 h265 streams
	bool send_nalu(const uint8_t *data, int length, uint64_t timestamp);

	bool is_ready() const noexcept {
		return running_.load(std::memory_order_acquire);
	}
	std::string rtsp_url() const;
	// Return one client URL for every active non-loopback IPv4 address.
	// The listening socket itself is bound to 0.0.0.0, so this list may be
	// refreshed after DHCP, link, or interface changes without restarting RTSP.
	std::vector<std::string> rtsp_urls() const;

private:
	std::atomic<bool>			   running_{false};
	void						  *context_{nullptr};
	std::shared_ptr<ThreadContext> ctx_;
	VideoCodecType				   codec_;
	std::string					   stream_name_;
	std::string					   rtsp_url_;
	int							   base_port  = 8554;
	int							   stream_id_ = 0;

	std::string	 user_;
	std ::string password_;

	std::mutex			 state_mutex_;
	std::thread			 server_thread_;
	std::vector<uint8_t> sps_data_;
	std::vector<uint8_t> pps_data_;
	std::vector<uint8_t> vps_data_;	 // for h 265 only
	uint32_t			 last_rtp_timestamp_{0};
	bool				 rtp_timestamp_initialized_{false};
};

#endif
