#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "AxVideoFrame.hpp"
#include "HwEncoder.hpp"

int main(int argc, char* argv[]) {
	int device_id  = 0;
	int channle_id = 0;
	if (argc == 3) {
		device_id  = atoi(argv[1]);
		channle_id = atoi(argv[2]);
	}
	auto SrcFrame = std::make_shared<AXVideoFrame>(1280, 720, device_id, AX_FORMAT_YUV420_SEMIPLANAR, 16);	// decode 256

	SrcFrame->load_data("1280x720_nv12.yuv");
	// AX_VIDEO_FRAME_T& srcFrame	= *SrcFrame;
	auto SrcFrame2 = std::make_shared<AXVideoFrame>(1920, 1080, device_id, AX_FORMAT_YUV420_SEMIPLANAR, 16);  // decode 256
	SrcFrame2->load_data("1920x1080.nv12");
	// 2 initialize the encoder
	auto encoder  = std::make_shared<HwEncoder>(device_id, 1, channle_id, 8554, "0");
	auto encoder2 = std::make_shared<HwEncoder>(device_id, 1, channle_id + 1, 8555, "1");

	for (int i = 0; i < 2000; ++i) {
		uint8_t nalu;
		size_t	nalu_size;
		encoder->Encode(SrcFrame->raw(), &nalu, nalu_size, PT_H264);
		encoder2->Encode(SrcFrame2->raw(), &nalu, nalu_size, PT_H264);
	}

	// for (int i = 0; i < 500; ++i) {
	// 	uint8_t nalu;
	// 	size_t	nalu_size;
	// 	encoder->Encode(&srcFrame, &nalu, nalu_size, PT_H264);
	// 	encoder2->Encode(&srcFrame, &nalu, nalu_size, PT_H264);
	// 	usleep(30000);
	// }
	printf("test done!\r\n");
	return 0;
}