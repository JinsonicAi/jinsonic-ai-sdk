#include <algorithm>
#include <iostream>

#include "JdkFireDetectNode.hpp"
#include "sdk_interface.hpp"

#ifndef PLUGIN_NODE_NAME
#define PLUGIN_NODE_NAME "firedet"
#endif

extern "C" void plugin_init(SDKInterface* sdk) {
	// std::cout << "[plugin] init with SDK interface" << std::endl;

	if (!sdk || !sdk->register_node) {
		std::cerr << "[plugin] ERROR: register_node is null!" << std::endl;
		return;
	}

	sdk->register_node(PLUGIN_NODE_NAME, [](const std::string& name, const nlohmann::json& config) {
		auto nodeParams = std::make_unique<jdk_nodes::FireNodeParams>();

		float threshold					= std::clamp(jp(config, "threshold", 80), 0, 100) / 100.0f;
		nodeParams->threshold			= (threshold > 0 && threshold < 1) ? threshold : 0.8f;
		nodeParams->show_fire			= jp(config, "show_fire", true);
		nodeParams->alarm_push_enable	= jp(config, "alarm_push_enable", true);
		nodeParams->alarm_push_interval = jp(config, "alarm_push_interval", 5);
		nodeParams->alarm_app_push		= jp(config, "alarm_app_push", 0);
		std::vector<int> recordPicList	= jp(config, "recordPicList", std::vector<int>{});
		for (int flag : recordPicList) {
			nodeParams->record_pic_type |= flag;
		}
		nodeParams->alarm_relay_enable	 = jp(config, "alarm_relay_enable", false);
		nodeParams->alarm_relay_interval = jp(config, "alarm_relay_interval", 5);

		nodeParams->device_id  = jp(config, "device_id", -1);
		nodeParams->model_path = jp(config, "model_path", "./models/fs_20241231_npu1.model");
		nodeParams->task_id	   = jp(config, "task_id", "0");

		nodeParams->llm_review.enabled				 = jp(config, "llm_review_enable", false);
		nodeParams->llm_review.prompt				 = jp(config, "llm_review_prompt", std::string("判断火焰是否真实存在，只回答 YES 或 NO。"));
			nodeParams->llm_review.expected_keyword	 = jp(config, "llm_review_expected_keyword", std::string("YES"));
			nodeParams->llm_review.timeout_ms			 = std::max(200, jp(config, "llm_review_timeout_ms", 3000));
		nodeParams->llm_review.deny_on_mismatch	 = jp(config, "llm_review_deny_on_mismatch", true);
		nodeParams->llm_review.follow_upstream_on_error = jp(config, "llm_review_follow_on_error", true);
		nodeParams->llm_review.decode_location		 = jp(config, "llm_review_decode_location", std::string(""));

	// TTS audio column broadcast configuration (fire/smoke)
		nodeParams->tts_fire.enabled = jp(config, "tts_fire_enabled", false);
		nodeParams->tts_fire.text    = jp(config, "tts_fire_text", std::string("警报！检测到烟雾或火焰，请注意安全并立即撤离！"));
		nodeParams->tts_fire.url     = jp(config, "tts_fire_url", std::string(""));

		return jdk_nodes::jdk_node_wrapper::create(
			name,
			std::make_shared<jdk_nodes::fireDetectNode>(
				name,
				std::move(nodeParams)));
	});
}

extern "C" void plugin_cleanup(SDKInterface* sdk) {
	if (sdk && sdk->unregister_node) {
		sdk->unregister_node(PLUGIN_NODE_NAME);
	} else {
		std::cerr << "[plugin] unregister node unbound skip logout\n";
	}
}
