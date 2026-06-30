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
#include "../sample_runtime.hpp"

int main(int argc, char* argv[]) {
	print_sample_runtime_usage(argv[0]);
	const auto runtime = parse_sample_runtime(argc, argv);
	auto frame = make_sample_video_frame(runtime, 1280, 886, AX_FORMAT_YUV420_SEMIPLANAR, 16);
	frame->load_data("1280x886_nv12.yuv", 1280 * 886 * 3 / 2);												 // 2048*1080*3/2
	frame->save_data("1280x886_nv12_dump.yuv");
	return 0;
}