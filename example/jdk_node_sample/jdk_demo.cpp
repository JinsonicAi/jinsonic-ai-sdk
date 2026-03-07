#include <unistd.h>

#include <string>

#include "node_factory.hpp"
#include "plugin_loader.hpp"
#include "sdk_interface.hpp"
//
#include <ax_engine_api.h>
#include <ax_ivps_api.h>
#include <ax_sys_api.h>
#include <axcl.h>

// #include "JdkNetlClientNode.hpp"
#include "ax_algorithm_sdk.h"

extern void setup_sdk(SDKInterface& sdk);
// extern int	ax_alg_init();

int main(int argc, char* argv[]) {
	int device_id = 0;
	if (argc == 2) {
		device_id = atoi(argv[1]);
	}
	jdk_nodes::set_log_level(jdk_nodes::jdk_log_level::DEBUG);
	SDKInterface sdk = {0};
	setup_sdk(sdk);	 // set callbacks for the sdk

	auto plugin_loader = std::make_shared<PluginLoader>();
	plugin_loader->load_all_plugins("./plugins_out", &sdk);
	auto Json = plugin_loader->get_component_structure();
	// printf("json:%s\r\n", Json.dump().data());
	// process_component_structure(Json);
	std::cout << std::setw(4) << Json << std::endl;
#if 0
	// int device_id = -1;
	// for (int i = 0; i < 10; i++) {
	// example json parameters from the frontend from a web page
	nlohmann::json node_config = {
		{"type", "netclient_plugin"},
		{"node_name", "client1"},
		{"rtsp_url", "rtsp://192.168.1.250:8554/test_1280x720_h264_aac.mp4"},
		{"device_id", device_id},
		{"channel_id", 1},
		{"task_id", "42"}};
	auto netclient = NodeFactory::instance().create(
		node_config["type"], node_config["node_name"], node_config);

	if (netclient) {
		printf("[Main] Created node: ---->node_name:%s\r\n", node_config["type"].dump().data());
	} else {
		std::cerr << "Unknown node type: " << node_config["type"] << std::endl;
	}

	// // faceDetect
	// nlohmann::json facedetect_config = {
	// 	{"type", "faceDetect_plugin"},
	// 	{"node_name", "faceDet1"},	// name
	// 	{"model_path", "./models/yolov5n-face.axmodel"},
	// 	{"device_id", device_id},
	// 	{"task_id", "2"}};
	// auto faceDet = NodeFactory::instance().create(
	// 	facedetect_config["type"], facedetect_config["node_name"], facedetect_config);
	// if (faceDet) {
	// 	printf("[Main] Created node: ---->node_name:%s\r\n", facedetect_config["type"].dump().data());
	// } else {
	// 	std::cerr << "Unknown node type: " << facedetect_config["type"] << std::endl;
	// }

	// // osd
	// nlohmann::json osd_config = {
	// 	{"type", "osd_plugin"},
	// 	{"node_name", "osd_plugin"},  // name
	// 	{"device_id", device_id},
	// 	{"task_id", "2"}};
	// auto Osd = NodeFactory::instance().create(
	// 	osd_config["type"], osd_config["node_name"], osd_config);
	// if (Osd) {
	// 	printf("[Main] Created node: ---->node_name:%s\r\n", osd_config["type"].dump().data());
	// } else {
	// 	std::cerr << "Unknown node type: " << osd_config["type"] << std::endl;
	// }

	// fireDetect
	nlohmann::json fire_config = {
		{"type", "fireDetect_plugin"},
		{"node_name", "fireDetect_plugin"},	 // name
		{"model_path", "./models/fs_20241231_npu1.model"},
		{"device_id", device_id},
		{"task_id", "2"}};
	auto fire = NodeFactory::instance().create(
		fire_config["type"], fire_config["node_name"], fire_config);
	if (fire) {
		printf("[Main] Created node: ---->node_name:%s\r\n", fire_config["type"].dump().data());
	} else {
		std::cerr << "Unknown node type: " << fire_config["type"] << std::endl;
	}

	// // lpr
	// nlohmann::json lpr_config = {
	// 	{"type", "LicensePlateReco_plugin"},
	// 	{"node_name", "LicensePlateReco_plugin"},	 // name
	// 	{"model_path", "./models/lpr_20241129_npu1.model"},
	// 	{"device_id", device_id},
	// 	{"task_id", "2"}};
	// auto lpr = NodeFactory::instance().create(
	// 	lpr_config["type"], lpr_config["node_name"], lpr_config);
	// if (fire) {
	// 	printf("[Main] Created node: ---->node_name:%s\r\n", lpr_config["type"].dump().data());
	// } else {
	// 	std::cerr << "Unknown node type: " << lpr_config["type"] << std::endl;
	// }

	// faceRec
	nlohmann::json faceRec_config = {
		{"type", "faceRecognition_plugin"},
		{"node_name", "faceRecognition_plugin"},  // name
		{"model_path", "./models/fr_20241219_npu1.model"},
		{"push", false},
		{"pushList", std::vector<int>{}},
		{"pushInterval", 0},
		{"device_id", device_id},
		{"task_id", "2"}};
	auto faceRec = NodeFactory::instance().create(
		faceRec_config["type"], faceRec_config["node_name"], faceRec_config);
	if (faceRec) {
		printf("[Main] Created node: ---->node_name:%s\r\n", faceRec_config["type"].dump().data());
	} else {
		std::cerr << "Unknown node type: " << faceRec_config["type"] << std::endl;
	}

	// hdmi
	nlohmann::json hdmi_config = {
		{"type", "hdmi_plugin"},
		{"node_name", "hdmi"},
		{"device_id", device_id},
		{"hdmi_device_id", 1},
		{"task_id", "42"}};
	auto hdmi = NodeFactory::instance().create(
		hdmi_config["type"], hdmi_config["node_name"], hdmi_config);

	if (hdmi) {
		printf("[Main] Created node: ---->node_name:%s\r\n", hdmi_config["type"].dump().data());
	} else {
		std::cerr << "Unknown node type: " << hdmi_config["type"] << std::endl;
	}

	// faceDet->attach_to({netclient});
	// Osd->attach_to({faceDet});
	// fire->attach_to({netclient});
	// lpr->attach_to({netclient});
	faceRec->attach_to({netclient});
	netclient->start();

	// }
	std::string wait;
	std::getline(std::cin, wait);

	if (auto ctrl = std::make_shared<jdk_objects::jdk_control_meta>(jdk_objects::jdk_control_type::SPEAK, 0); ctrl) {
		netclient->handle_control_meta(ctrl);
	}
	netclient->detach_recursively();
#endif
	printf(" main exit\r\n");
	return 0;
}
