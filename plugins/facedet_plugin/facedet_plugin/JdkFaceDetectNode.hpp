#pragma once

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <string>

#include "JdkOsd.hpp"
#include "MetricsReporter.hpp"
#include "jdk_node_base.hpp"
#include "jdk_node_wrapper.hpp"
#include "yoloFace.hpp"

namespace jdk_nodes {
class faceDetectV2Node : public jdk_node_base, public CustomHandleFrame, public CustomHandleControl {
public:
	faceDetectV2Node(std::string node_name,
					 const std::string& filename,
					 float threshold,
					 int device_id,
					 std::string task_id = "",
					 int label_score_step = 5);
	~faceDetectV2Node();
	void stop();

protected:
	// we can define new logic for infer operations by overriding it.
	virtual void run_infer_combinations(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& frame_meta_with_batch);

	// re-implementation for one by one mode, marked as 'final' as we need not override any more in specific derived classes.
	virtual std::shared_ptr<jdk_objects::jdk_meta> handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) override final;
	// re-implementation for batch by batch mode, marked as 'final' as we need not override any more in specific derived classes.
	virtual void								   handle_frame_meta(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& meta_with_batch) override final;
	virtual std::shared_ptr<jdk_objects::jdk_meta> handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) override;
	// bool has_custom_handle_frame() const override { return true; }

private:
	std::pair<nlohmann::json, std::vector<std::function<std::shared_ptr<AXVideoFrame>()>>>
									 alarm_fn(const std::any& future_any, std::shared_ptr<AXVideoFrame> canvas);
	void							 render_fn(std::shared_ptr<AXVideoFrame>& canvas, const std::any& future_any, const std::any& extra = {});
	jdk_osd::Overlay				 build_overlay_(const YOLOFACE::Objects& det);
	std::shared_ptr<YOLOFACE::Infer> infer;
	float							 threshold_ = 0.7;
	int								 device_id_ = -1;
	int								 label_score_step_ = 5;
	std::string						 task_id_{};
	int								 channel_id_;  // default 0
	std::mutex						 mutex_;
	MetricsReporter					 reporter_{5};
};
}  // namespace jdk_nodes
