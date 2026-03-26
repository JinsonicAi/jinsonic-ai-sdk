#include <signal.h>
#include <stdio.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "HwDecoder.hpp"
#include "NetClient.hpp"

int main(int argc, char *argv[]) {
	int device_id  = 0;
	int channle_id = 0;
	if (argc == 3) {
		device_id  = atoi(argv[1]);
		channle_id = atoi(argv[2]);
	}
	stream_info info;
	auto		netclient = std::make_shared<NetClient>(device_id, channle_id, 0, info, "");
	netclient->start("rtsp://admin:123456@192.168.1.15/ch01.264");
	// rtsp://admin:admin@192.168.1.12:554/live0.264
	// auto netclient1 = std::make_shared<NetClient>(device_id, channle_id + 1, 0);
	// netclient->start("rtsp://admin:123456@192.168.1.250:8554/test_1280x720_h264_aac.mp4");
	// netclient1->start("rtsp://admin:123456@192.168.1.250:8554/bangkok_30952_1920x1080_30fps_gop60_4Mbps_h265.mp4");
	// stream_info info;
	// info.video.fps	   = 30;
	// info.video.width   = 1920;
	// info.video.height  = 1080;
	// info.video.payload = (AX_PAYLOAD_TYPE_E)96;
	// auto decoder	   = std::make_shared<HwDecoder>(device_id, 0, 0, info);

	std::string wait;
	std::getline(std::cin, wait);

	// decoder.reset();

	netclient.reset();
	return 0;
}