#ifndef __RTSP_SERVER_HANDLE_H__
#define __RTSP_SERVER_HANDLE_H__
#include <cstdio>
#include <cstring>
#include <future>
#include <iostream>
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

	std::string rtsp_url() const;

private:
	// make sure the nalu type matches H264/H265
	bool						   is_valid_nalu(uint8_t *data, int length);
	std::atomic<bool>			   running_{true};
	void						  *context_{nullptr};
	std::shared_ptr<ThreadContext> ctx_;
	VideoCodecType				   codec_;
	std::string					   stream_name_;
	std::string					   rtsp_url_;
	int							   base_port  = 8554;
	int							   stream_id_ = 0;

	std::string	 user_;
	std ::string password_;

	bool				 first_frame_ = true;
	std::vector<uint8_t> sps_data_;
	std::vector<uint8_t> pps_data_;
	std::vector<uint8_t> vps_data_;	 // for h 265 only
};

#endif