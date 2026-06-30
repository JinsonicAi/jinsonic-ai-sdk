#include <signal.h>
#include <stdio.h>

#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "AxVideoFrame.hpp"
#include "HwIvps.hpp"
#include "../sample_runtime.hpp"

int main(int argc, char* argv[]) {
	print_sample_runtime_usage(argv[0]);
	const auto runtime = parse_sample_runtime(argc, argv);
	std::cout << "runtime_location=" << runtime.location << ", runtime_device_id=" << runtime.runtime_device_id << std::endl;

	auto ipvs = std::make_shared<HwIvps>(runtime.runtime_device_id, 0, 0, runtime.location);

	auto DewarpFrame = make_sample_video_frame(runtime, 1280, 720, AX_FORMAT_YUV420_SEMIPLANAR, 16);
	DewarpFrame->load_data("pre_process_1280x720_nv12.yuv", 1280 * 720 * 3 / 2);							   // 2048*1080*3/2
	// AX_VIDEO_FRAME_T& dewarpFrame = *DewarpFrame;

	// dewarpFrame.u32PicStride[1] = 2048;
	DewarpFrame->save_data("1280x720_decoder.nv12");
	DewarpFrame->raw()->u32Width = 1280;
	// auto DewarpFrame = std::make_shared<AXVideoFrame>(1920, 1080, AX_FORMAT_YUV420_SEMIPLANAR, 16);
	// DewarpFrame->load_data("data/ivps/1920x1080.nv12");
	// AX_VIDEO_FRAME_T& dewarpFrame = *DewarpFrame;
	// dewarpFrame.u32PicStride[0]	  = 2048;
	// dewarpFrame.u32PicStride[1]	  = 2048;

	TransformMatrices tm = {0};

	auto dstDewarpFrame = ipvs->HwIvpsDewarp(DewarpFrame->raw(), {416, 416}, tm, ResizeOptions());

	if (dstDewarpFrame == nullptr) {
		printf("HwIvpsDewarp failed.\n");
		// return -1;
	}

	dstDewarpFrame->save_data("416x416_nv12_Dewarp1.yuv");
	//  NV12 TO RGB
	auto RgbFrame = ipvs->HwIvpsCsc(dstDewarpFrame->raw());
	if (!RgbFrame) {
		std::cout << "Valid RgbFrame\n";
		return false;
	}
	RgbFrame->save_data("416x416_nv12_Dewarp.rgb");
	return 0;
}