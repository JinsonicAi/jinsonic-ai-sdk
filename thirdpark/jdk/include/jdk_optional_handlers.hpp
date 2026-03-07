#pragma once

#include <memory>
#include <thread>

#include "jdk_control_meta.hpp"
#include "jdk_frame_meta.hpp"

namespace jdk_nodes {

struct CustomHandleRun {
	virtual void handle_run(std::stop_token stoken) = 0;
	virtual ~CustomHandleRun()						= default;
};

struct CustomHandleFrame {
	virtual std::shared_ptr<jdk_objects::jdk_meta> handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) = 0;
	// default empty implementation optional for batch processing
	virtual void handle_frame_meta(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& meta_with_batch) {
		for (const auto& meta : meta_with_batch) {
			handle_frame_meta(meta);
		}
	}
	virtual ~CustomHandleFrame() = default;
};

struct CustomHandleControl {
	virtual std::shared_ptr<jdk_objects::jdk_meta> handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) = 0;
	virtual ~CustomHandleControl()																							= default;
};

}  // namespace jdk_nodes
