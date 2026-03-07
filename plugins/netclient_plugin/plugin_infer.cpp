#include <iostream>

#include "JdkNetlClientNode.hpp"
#include "sdk_interface.hpp"

#ifndef PLUGIN_NODE_NAME
#define PLUGIN_NODE_NAME "netclient"
#endif

extern "C" void plugin_init(SDKInterface* sdk) {
	// std::cout << "[plugin] init with SDK interface" << std::endl;

	if (!sdk || !sdk->register_node) {
		std::cerr << "[plugin] ERROR: register_node is null!" << std::endl;
		return;
	}

	sdk->register_node(PLUGIN_NODE_NAME, [](const std::string& name, const nlohmann::json& config) {
		stream_info info{};
		info.video.payload = static_cast<AX_PAYLOAD_TYPE_E>(jp(config, "payload", 96));
		info.video.width   = jp(config, "width", 0);
		info.video.height  = jp(config, "height", 0);

		// 提取按时布控配置（可选）
		nlohmann::json schedule_config = config.value("schedule_config", nlohmann::json{});

		return jdk_nodes::jdk_node_wrapper::create(
			name,
			std::make_shared<jdk_nodes::NetClientNode>(
				name,
				jp(config, "rtsp_url", ""),
				jp(config, "device_id", -1),
				jp(config, "channel_id", 0),
				jp(config, "channel", 0),
				info,
				jp(config, "task_id", "0"),
				jp(config, "task_name", "netclient"),
				schedule_config));
	});
}

extern "C" void plugin_cleanup(SDKInterface* sdk) {
	if (sdk && sdk->unregister_node) {
		sdk->unregister_node(PLUGIN_NODE_NAME);
	} else {
		std::cerr << "[plugin] unregister node unbound skip logout\n";
	}
}
