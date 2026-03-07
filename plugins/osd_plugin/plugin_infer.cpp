#include <iostream>

#include "JdkOsdNode.hpp"
#include "sdk_interface.hpp"

#ifndef PLUGIN_NODE_NAME
#define PLUGIN_NODE_NAME "osd"
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
			std::make_shared<jdk_nodes::osdNode>(
				name,
				jp(config, "device_id", -1),
				jp(config, "task_id", "0")));
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