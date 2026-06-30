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
#include "../sample_runtime.hpp"

int main(int argc, char* argv[]) {
	print_sample_runtime_usage(argv[0]);
	const auto runtime = parse_sample_runtime(argc, argv);
	int channle_id = 0;
	if (argc >= 3) {
		channle_id = atoi(argv[2]);
	}
	std::cout << "runtime_location=" << runtime.location << ", runtime_device_id=" << runtime.runtime_device_id << std::endl;
	auto SrcFrame = make_sample_video_frame(runtime, 1280, 720, AX_FORMAT_YUV420_SEMIPLANAR, 16);

	SrcFrame->load_data("1280x720_nv12.yuv");
	// AX_VIDEO_FRAME_T& srcFrame	= *SrcFrame;
	auto SrcFrame2 = make_sample_video_frame(runtime, 1920, 1080, AX_FORMAT_YUV420_SEMIPLANAR, 16);
	SrcFrame2->load_data("1920x1080.nv12");
	// 2 initialize the encoder
	auto encoder  = std::make_shared<HwEncoder>(runtime.runtime_device_id, 1, channle_id, 8554, "0", runtime.location);
	auto encoder2 = std::make_shared<HwEncoder>(runtime.runtime_device_id, 1, channle_id + 1, 8555, "1", runtime.location);

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