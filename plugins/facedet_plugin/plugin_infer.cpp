#include <iostream>
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
		const auto rk_model = jp(config, "model_path_rk", std::string{});
		if (!rk_model.empty())
			return rk_model;

		const auto generic_model = jp(config, "model_path", std::string{});
		if (generic_model.size() >= 5 && generic_model.compare(generic_model.size() - 5, 5, ".rknn") == 0)
			return generic_model;

		if (!generic_model.empty()) {
			std::cerr << "[FaceDet] runtime rk.local ignores non-RKNN model_path=" << generic_model
					  << "; use model_path_rk for a board-specific .rknn model" << std::endl;
		}
		return "./models/yolov5n-face_rk3588.rknn";
	}

	const auto ax_model = jp(config, "model_path_ax", std::string{});
	return ax_model.empty() ? jp(config, "model_path", "./models/yolov5n-face.axmodel") : ax_model;
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
		// Keep the display threshold consistent with the detector's default
		// confidence threshold. Missing UI state must not hide post-NMS faces.
		const float threshold = std::clamp(jp(config, "threshold", 0.45f), 0.05f, 0.95f);
		const auto model_path = select_model_path(config, runtime);
		return jdk_nodes::jdk_node_wrapper::create(
			name,
			std::make_shared<jdk_nodes::faceDetectV2Node>(
				name,
				model_path,
				threshold,
				runtime,
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
