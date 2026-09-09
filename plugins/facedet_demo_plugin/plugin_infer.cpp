#include <iostream>
#include <stdexcept>
#include <string>

#include "JdkFaceDetectNode.hpp"
#include "PluginRuntime.hpp"
#include "sdk_interface.hpp"

#ifndef PLUGIN_NODE_NAME
#define PLUGIN_NODE_NAME "facedet"
#endif

namespace {
std::string select_model_path(const nlohmann::json& config, const PluginRuntime& runtime) {
	if (runtime.is_rk_local()) {
		return PluginRuntime::require_model_file_with_extension(config, ".rknn");
	}
	return PluginRuntime::require_model_file_with_extension(config, ".axmodel");
}
}  // namespace

extern "C" void plugin_init(SDKInterface* sdk) {
	// std::cout << "[plugin] init with SDK interface" << std::endl;

	if (!sdk || !sdk->register_node) {
		std::cerr << "[plugin] ERROR: register_node is null!" << std::endl;
		return;
	}

	sdk->register_node(PLUGIN_NODE_NAME, [](const std::string& name, const nlohmann::json& config) {
		const auto runtime = PluginRuntime::from_task_config(config);
		// "threshold" remains a compatibility fallback for existing tasks.
		const float threshold = std::clamp(jp(config, "face_threshold", jp(config, "threshold", 0.45f)), 0.05f, 0.95f);
		const auto model_path = select_model_path(config, runtime);
		return jdk_nodes::jdk_node_wrapper::create(
			name,
			std::make_shared<jdk_nodes::faceDetectV2Node>(
				name,
				model_path,
				threshold,
				runtime,
				jp(config, "task_id", "0"),
				std::clamp(jp(config, "osd_label_score_step", 5), 0, 100),
				std::clamp(jp(config, "confirm_frames", 3), 1, 120),
				std::clamp(jp(config, "track_iou", 0.30f), 0.05f, 0.95f),
				std::clamp(jp(config, "track_max_missed", 30), 1, 600),
				std::clamp(jp(config, "alarm_push_interval_ms", 10000), 1000, 600000)));
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
