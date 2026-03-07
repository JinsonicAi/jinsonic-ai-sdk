#ifndef _HW_CAPTURE_HPP_
#define _HW_CAPTURE_HPP_

#include <signal.h>
#include <stdio.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "AxVideoFrame.hpp"

class HwCapture {
public:
	HwCapture(int device_id);
	~HwCapture();

	std::shared_ptr<AXVideoFrame> capture(AX_VIDEO_FRAME_T* srcFrame, __u32 QPLevel = 90);
	std::shared_ptr<AXVideoFrame> decodeJpegToNV12(
		const uint8_t* jpeg_buf,
		size_t		   jpeg_size);

private:
	int	 device_id_;
	bool isHost();
	// void  SysDeinit();
	// int	  SysInit();
	// void* context_ = nullptr;
};

#endif	// _HW_DECODER_HPP_