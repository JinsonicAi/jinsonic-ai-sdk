#include <iostream>

#include "JdkFaceDetectNode.hpp"
#include "sdk_interface.hpp"

#ifndef PLUGIN_NODE_NAME
#define PLUGIN_NODE_NAME "facedet"
#endif

extern "C" void plugin_init(SDKInterface* sdk) {
	// std::cout << "[plugin] init with SDK interface" << std::endl;

	if (!sdk || !sdk->register_node) {
		std::cerr << "[plugin] ERROR: register_node is null!" << std::endl;
		return;
	}

	sdk->register_node(PLUGIN_NODE_NAME, [](const std::string& name, const nlohmann::json& config) {
		return jdk_nodes::jdk_node_wrapper::create(
			name,
			std::make_shared<jdk_nodes::faceDetectV2Node>(
				name,
				jp(config, "model_path", "./models/yolov5n-face.axmodel"),
				jp(config, "threshold", 0.7f),
				jp(config, "device_id", -1),
				jp(config, "task_id", "0"),
				std::clamp(jp(config, "osd_label_score_step", 5), 0, 100)));
		;
	});
}

extern "C" void plugin_cleanup(SDKInterface* sdk) {
	if (sdk && sdk->unregister_node) {
		sdk->unregister_node(PLUGIN_NODE_NAME);
	} else {
		std::cerr << "[plugin] unregister node unbound skip logout\n";
	}
}
