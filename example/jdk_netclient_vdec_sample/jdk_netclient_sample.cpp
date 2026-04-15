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
	// int device_id  = 0;
	// int channle_id = 0;
	// if (argc == 3) {
	// 	device_id  = atoi(argv[1]);
	// 	channle_id = atoi(argv[2]);
	// }
	// stream_info info;
	// auto		netclient = std::make_shared<NetClient>(device_id, channle_id, 0, info, "");
	// netclient->start("rtsp://admin:123456@192.168.1.15/ch01.264");

    // frame = netclient->get_frame((std::atomic<bool>*)alive_ptr());
	// std::string wait;
	// std::getline(std::cin, wait);

	// // decoder.reset();

	// netclient.reset();
	// return 0;
    int device_id = 0;
    int channel_id = 0;

    if (argc == 3) {
        device_id = atoi(argv[1]);
        channel_id = atoi(argv[2]);
    }

    stream_info info;
    auto netclient = std::make_shared<NetClient>(device_id, channel_id, 0, info, "");

    netclient->start("rtsp://192.168.1.250:8554/fire");

    std::atomic<bool> alive(true);

    // 输入线程：按回车退出
    std::thread input_thread([&alive]() {
        std::cout << "按回车退出..." << std::endl;
        std::string wait;
        std::getline(std::cin, wait);
        alive = false;
    });

    // 循环取帧
    while (alive.load()) {
        auto frame = netclient->get_frame(&alive);

        if (frame) {
            std::cout << "get frame success: " << frame.get() << std::endl;
        } else {
            std::cout << "get frame failed or null" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    if (input_thread.joinable()) {
        input_thread.join();
    }
    netclient->stop();
    netclient.reset();
    return 0;
}