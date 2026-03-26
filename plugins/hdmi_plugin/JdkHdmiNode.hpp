#pragma once

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <string>

#include "HwIvps.hpp"
#include "MetricsReporter.hpp"
#include "jdk_node_base.hpp"
#include "jdk_node_wrapper.hpp"

namespace jdk_nodes {
class HdmiNode : public jdk_node_base, public CustomHandleFrame, public CustomHandleControl {
public:
	HdmiNode(std::string node_name, int device_id, int hdmi_device_id = 0, std::string task_id = "");
	~HdmiNode();
	void stop();

protected:
	// // we can define new logic for infer operations by overriding it.
	// virtual void run_infer_combinations(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& frame_meta_with_batch);

	// re-implementation for one by one mode, marked as 'final' as we need not override any more in specific derived classes.
	virtual std::shared_ptr<jdk_objects::jdk_meta> handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) override final;
	// re-implementation for batch by batch mode, marked as 'final' as we need not override any more in specific derived classes.
	virtual void								   handle_frame_meta(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& meta_with_batch) override final;
	virtual std::shared_ptr<jdk_objects::jdk_meta> handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) override;
	// bool has_custom_handle_frame() const override { return true; }

private:
	std::shared_ptr<HwIvps> ivps_	   = nullptr;
	int						device_id_ = -1;
	std::string				task_id_{};

	int				hdmi_device_id_ = 0;
	std::mutex		mutex_;
	MetricsReporter reporter_{5};
};
}  // namespace jdk_nodes