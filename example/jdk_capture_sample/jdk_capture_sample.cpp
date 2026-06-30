#include <signal.h>
#include <stdio.h>

#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "HwCapture.hpp"
#include "../sample_runtime.hpp"

int main(int argc, char* argv[]) {
	print_sample_runtime_usage(argv[0]);
	const auto runtime = parse_sample_runtime(argc, argv);
	if (runtime.is_rk_local()) {
		std::cerr << "HwCapture standalone sample uses the legacy capture constructor; "
				  << "RK local snapshots should be validated through frame->save_data() or capture-enabled plugins." << std::endl;
	}
	auto Capture  = std::make_shared<HwCapture>(runtime.runtime_device_id);
	auto Capture1 = std::make_shared<HwCapture>(runtime.runtime_device_id);
	auto Capture2 = std::make_shared<HwCapture>(runtime.runtime_device_id);
	auto Capture3 = std::make_shared<HwCapture>(runtime.runtime_device_id);
	auto Capture4 = std::make_shared<HwCapture>(runtime.runtime_device_id);
	auto Capture5 = std::make_shared<HwCapture>(runtime.runtime_device_id);

	auto SrcFrame = make_sample_video_frame(runtime, 1920, 1080, AX_FORMAT_YUV420_SEMIPLANAR, 16);
	printf(" func:%s, line:%d\r\n", __FUNCTION__, __LINE__);
	SrcFrame->load_data("data/ivps/1920x1080.nv12");
	auto jpegFrame = Capture->capture(SrcFrame->raw());
	if (jpegFrame) {
		jpegFrame->save_data("Capture_jpegFrame.jpg");
	}
	printf("test done!\r\n");
	return 0;
}