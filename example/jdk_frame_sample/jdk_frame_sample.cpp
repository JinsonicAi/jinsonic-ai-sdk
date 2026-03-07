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

int main(int argc, char* argv[]) {
	int	 device_id = -1;
	auto frame	   = std::make_shared<AXVideoFrame>(1280, 886, device_id, AX_FORMAT_YUV420_SEMIPLANAR, 16);	 // decode 256
	frame->load_data("1280x886_nv12.yuv", 1280 * 886 * 3 / 2);												 // 2048*1080*3/2
	frame->save_data("1280x886_nv12_dump.yuv");
	return 0;
}