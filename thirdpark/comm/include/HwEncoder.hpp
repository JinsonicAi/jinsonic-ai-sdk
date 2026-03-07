#ifndef _HW_DECODER_HPP_
#define _HW_DECODER_HPP_

#include <signal.h>
#include <stdio.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

// #include "AXCLEncoder.hpp"
// #include "AXEncoder.hpp"
#include "IEncoder.hpp"
// #include "RtspServer.hpp"
#include "rtsp_server.hpp"

class HwEncoder {
public:
	HwEncoder(int device_id, int group, int channel, int rtsp_port, std::string task_id);
	~HwEncoder();

	std::shared_ptr<AXVideoFrame> Encode(AX_VIDEO_FRAME_T* srcFrame, const uint8_t* nalu, size_t nalu_size, AX_PAYLOAD_TYPE_E enType = PT_H265);

	// std::string rtsp_url() const;

private:
	std::string				  task_id_;
	int						  device_id_;
	int						  group_;
	int						  channel_id_;
	int						  rtsp_port_;
	std::shared_ptr<IEncoder> encoder_{nullptr};
	// std::shared_ptr<RtspServer> rtsp_{nullptr};
};

#endif	// _HW_DECODER_HPP_