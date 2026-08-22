#pragma once

#include <memory>
#include <thread>

#include "jdk_control_meta.hpp"
#include "jdk_frame_meta.hpp"

namespace jdk_nodes {

struct CustomHandleRun {
	virtual void handle_run(std::stop_token stoken) = 0;
	virtual ~CustomHandleRun() = default;
};

struct CustomHandleFrame {
	virtual std::shared_ptr<jdk_objects::jdk_meta> handle_frame_meta(
		std::shared_ptr<jdk_objects::jdk_frame_meta> meta) = 0;
	virtual void handle_frame_meta(
		const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& meta_with_batch) {
		for (const auto& meta : meta_with_batch) handle_frame_meta(meta);
	}
	virtual ~CustomHandleFrame() = default;
};

// Optional fast path for consumers that combine a source video stream with
// asynchronous algorithm results.  Returning true consumes the result as
// sideband metadata, keeping it out of the source-frame DropOldest queue.
struct CustomHandleSidebandFrame {
	virtual bool handle_sideband_frame(
		const std::shared_ptr<jdk_objects::jdk_frame_meta>& meta) = 0;
	virtual ~CustomHandleSidebandFrame() = default;
};

struct CustomHandleControl {
	virtual std::shared_ptr<jdk_objects::jdk_meta> handle_control_meta(
		std::shared_ptr<jdk_objects::jdk_control_meta> meta) = 0;
	virtual ~CustomHandleControl() = default;
};

}  // namespace jdk_nodes
