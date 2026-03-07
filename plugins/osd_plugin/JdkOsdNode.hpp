#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <vector>

#include "HwIvps.hpp"
#include "MetricsReporter.hpp"
#include "jdk_node_base.hpp"
#include "jdk_node_wrapper.hpp"

namespace jdk_nodes {
class osdNode : public jdk_node_base, public CustomHandleFrame, public CustomHandleControl {
public:
	osdNode(std::string node_name, int device_id, std::string task_id = "");
	~osdNode();
	void stop();

protected:
	// // we can define new logic for infer operations by overriding it.
	// virtual void run_infer_combinations(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& frame_meta_with_batch);

	// re-implementation for one by one mode, marked as 'final' as we need not override any more in specific derived classes.
	virtual std::shared_ptr<jdk_objects::jdk_meta> handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) override final;
	// re-implementation for batch by batch mode, marked as 'final' as we need not override any more in specific derived classes.
	virtual void handle_frame_meta(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& meta_with_batch) override final;

	// bool has_custom_handle_frame() const override { return true; }
	virtual std::shared_ptr<jdk_objects::jdk_meta> handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) override;

private:
	std::shared_ptr<HwIvps> ivps_	   = nullptr;
	int						device_id_ = -1;
	std::string				task_id_{};
	std::mutex				mutex_;
	//
	std::atomic<std::shared_ptr<jdk_objects::result_map_t>> latest_result_map_snap_{
		std::make_shared<jdk_objects::result_map_t>()};

	void update_latest_result_(const std::string&							 key,
							   const jdk_objects::result_map_t::mapped_type& entry_sp);

	std::shared_ptr<const jdk_objects::result_map_t> load_latest_result_snapshot_() const;
	MetricsReporter									 reporter_{5};
};
}  // namespace jdk_nodes