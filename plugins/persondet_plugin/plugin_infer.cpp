#include <algorithm>
#include <iostream>

#include "JdkPersonDetectNode.hpp"
#include "sdk_interface.hpp"

#ifndef PLUGIN_NODE_NAME
#define PLUGIN_NODE_NAME "persondet"
#endif

extern "C" void plugin_init(SDKInterface* sdk) {
	// std::cout << "[plugin] init with SDK interface" << std::endl;

	if (!sdk || !sdk->register_node) {
		std::cerr << "[plugin] ERROR: register_node is null!" << std::endl;
		return;
	}

	sdk->register_node(PLUGIN_NODE_NAME, [](const std::string& name, const nlohmann::json& config) {
		auto nodeParams = std::make_unique<jdk_nodes::PersonDetParams>();

		float threshold					= std::clamp(jp(config, "threshold", 80), 0, 100) / 100.0f;
		nodeParams->threshold			= threshold > 0 ? threshold : 0.8f;
		nodeParams->show_face			= jp(config, "show_face", true);
		nodeParams->show_human			= jp(config, "show_human", true);
		nodeParams->mini_face			= jp(config, "mini_face", 30);
		float quality_face				= std::clamp(jp(config, "quality_face", 0), 0, 100) / 100.0f;
		nodeParams->quality_face		= (quality_face > 0 && quality_face < 1) ? quality_face : 0.4f;
		nodeParams->mini_human			= jp(config, "mini_human", 30);
		float quality_human				= std::clamp(jp(config, "quality_human", 0), 0, 100) / 100.0f;
		nodeParams->quality_human		= (quality_human > 0 && quality_human < 1) ? quality_human : 0.0f;
		nodeParams->cap_mode			= jp(config, "cap_mode", 0);
		nodeParams->cap_interval		= jp_clamped(config, "cap_interval", 10, 1, 500);
		nodeParams->alarm_push_enable	= jp(config, "alarm_push_enable", true);
		nodeParams->alarm_push_interval = jp_clamped(config, "alarm_push_interval", 5, 1, 3600);
		nodeParams->alarm_app_push		= jp(config, "alarm_app_push", 0);
		nodeParams->server_push_enable	= jp(config, "server_push_enable", false);
		std::vector<int> serverPicList	= jp(config, "serverPicList", std::vector<int>{});
		for (int flag : serverPicList) {
			nodeParams->server_pic_type |= flag;
		}
		nodeParams->server_attr_enable = jp(config, "server_attr_enable", false);
		std::vector<int> recordPicList = jp(config, "recordPicList", std::vector<int>{});
		for (int flag : recordPicList) {
			nodeParams->record_pic_type |= flag;
		}
		nodeParams->alarm_relay_enable	 = jp(config, "alarm_relay_enable", false);
		nodeParams->alarm_relay_interval = jp_clamped(config, "alarm_relay_interval", 5, 1, 3600);

		// Region rule configuration (takes effect when user draws regions/lines)
		nodeParams->region_rule.enable_enter	  = jp(config, "region_enable_enter", true);
		nodeParams->region_rule.enable_leave	  = jp(config, "region_enable_leave", true);
		nodeParams->region_rule.enable_loiter	  = jp(config, "region_enable_loiter", false);
		nodeParams->region_rule.loiter_sec		  = jp_clamped(config, "region_loiter_sec", 10, 1, 86400);
		nodeParams->region_rule.loiter_cooldown	  = jp_clamped(config, "region_loiter_cooldown", 30, 0, 86400);
		nodeParams->region_rule.enable_absence	  = jp(config, "region_enable_absence", false);
		nodeParams->region_rule.absence_sec		  = jp_clamped(config, "region_absence_sec", 30, 1, 86400);
		nodeParams->region_rule.absence_cooldown  = jp_clamped(config, "region_absence_cooldown", 0, 0, 86400);
		nodeParams->region_rule.enable_crowd	  = jp(config, "region_enable_crowd", false);
		nodeParams->region_rule.crowd_threshold	  = jp_clamped(config, "region_crowd_threshold", 10, 1, 100000);
		nodeParams->region_rule.crowd_recover_gap = jp_clamped(config, "region_crowd_recover_gap", 2, 0, 100000);
		nodeParams->region_rule.enable_line_cross = jp(config, "region_enable_line_cross", false);
		nodeParams->region_rule.label_filter	  = jp_clamped(config, "region_label_filter", -1, -1, 100000);
		// Default min bbox size comes from mini_human unless user overrides.
		nodeParams->region_rule.min_bbox_size = jp_clamped(config, "region_min_bbox_size", nodeParams->mini_human, 0, 100000);
		nodeParams->region_track_timeout_sec  = jp(config, "region_track_timeout_sec", 10.0f);

		nodeParams->device_id  = jp(config, "device_id", -1);
		nodeParams->model_path = jp(config, "model_path", "./models/person_20241206_npu1.model");
		nodeParams->task_id	   = jp(config, "task_id", "0");

		nodeParams->llm_review.enabled					= jp(config, "llm_review_enable", false);
		nodeParams->llm_review.prompt					= jp(config, "llm_review_prompt", std::string("判断图片中是否存在行人目标，只回答 YES 或 NO。"));
		nodeParams->llm_review.expected_keyword			= jp(config, "llm_review_expected_keyword", std::string("YES"));
		nodeParams->llm_review.timeout_ms				= jp_clamped(config, "llm_review_timeout_ms", 1200, 200, 600000);
		nodeParams->llm_review.deny_on_mismatch			= jp(config, "llm_review_deny_on_mismatch", true);
		nodeParams->llm_review.follow_upstream_on_error = jp(config, "llm_review_follow_on_error", true);
		nodeParams->llm_review.decode_location			= jp(config, "llm_review_decode_location", std::string(""));

		return jdk_nodes::jdk_node_wrapper::create(
			name,
			std::make_shared<jdk_nodes::personDetectNode>(
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
