#include <iostream>

#include "JdkAlarmNode.hpp"
#include "sdk_interface.hpp"

#ifndef PLUGIN_NODE_NAME
#define PLUGIN_NODE_NAME "alarm"
#endif

extern "C" void plugin_init(SDKInterface* sdk) {
	// std::cout << "[plugin] init with SDK interface" << std::endl;

	if (!sdk || !sdk->register_node) {
		std::cerr << "[plugin] ERROR: register_node is null!" << std::endl;
		return;
	}

	sdk->register_node(PLUGIN_NODE_NAME, [](const std::string& name, const nlohmann::json& config) {
		// 解析TTS配置
		jdk_nodes::TTSConfig tts_config;
		tts_config.enabled      = jp(config, "tts_enabled", false);
		tts_config.speaker_ip   = jp(config, "tts_speaker_ip", std::string(""));
		tts_config.volume       = jp(config, "tts_volume", 50);
		tts_config.speed        = jp(config, "tts_speed", 50);
		tts_config.vcn          = jp(config, "tts_vcn", std::string("xiaoyan"));
		tts_config.debounce_ms  = jp(config, "tts_debounce_ms", 5000);
		
		return jdk_nodes::jdk_node_wrapper::create(
			name,
			std::make_shared<jdk_nodes::alarmNode>(
				name,
				jp(config, "device_id", -1),
				jp(config, "http_url", ""),
				jp(config, "interval_sec", 3),
				jp(config, "task_id", "0"),
				jp(config, "task_name", PLUGIN_NODE_NAME),
				tts_config));
	});
}

extern "C" void plugin_cleanup(SDKInterface* sdk) {
	if (sdk && sdk->unregister_node) {
		sdk->unregister_node(PLUGIN_NODE_NAME);
	} else {
		std::cerr << "[plugin] unregister node unbound skip logout\n";
	}
}