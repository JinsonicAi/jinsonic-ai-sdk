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

int main(int argc, char* argv[]) {
	int device_id = -1;
	if (argc == 2) {
		device_id = atoi(argv[1]);
	}
	auto Capture  = std::make_shared<HwCapture>(device_id);
	auto Capture1 = std::make_shared<HwCapture>(device_id);
	auto Capture2 = std::make_shared<HwCapture>(device_id);
	auto Capture3 = std::make_shared<HwCapture>(device_id);
	auto Capture4 = std::make_shared<HwCapture>(device_id);
	auto Capture5 = std::make_shared<HwCapture>(device_id);

	auto SrcFrame = std::make_shared<AXVideoFrame>(1920, 1080, device_id, AX_FORMAT_YUV420_SEMIPLANAR, 16);	 // decode 256
	printf(" func:%s, line:%d\r\n", __FUNCTION__, __LINE__);
	SrcFrame->load_data("data/ivps/1920x1080.nv12");
	auto jpegFrame = Capture->capture(SrcFrame->raw());
	if (jpegFrame) {
		jpegFrame->save_data("Capture_jpegFrame.jpg");
	}
	printf("test done!\r\n");
	return 0;
}