#pragma once

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <deque>
#include <string>
#include <unordered_map>

#include "HwCapture.hpp"
#include "HwIvps.hpp"
#include "JdkPersonDetectNode.hpp"
#include "MetricsReporter.hpp"
#include "RegionAnalyzer.hpp"
#include "alg_comm.hpp"
#include "ax_algorithm_sdk.h"
#include "jdk_node_base.hpp"
#include "jdk_node_wrapper.hpp"

namespace jdk_nodes {

struct PersonLLMReviewConfig {
	bool		enabled{false};
	std::string prompt{};
	std::string expected_keyword{"YES"};
	int			timeout_ms{1200};
	bool		deny_on_mismatch{true};
	bool		follow_upstream_on_error{true};
	std::string decode_location{};
};

struct PersonDetParams {
	float		threshold{0.8f};  // range: 0.0-1.0
	bool		show_face{true};
	bool		show_human{true};
	int			mini_face{30};
	float		quality_face{0};
	int			mini_human{30};
	float		quality_human{0};
	int			cap_mode{2};
	int			cap_interval{10};
	bool		alarm_push_enable{true};
	int			alarm_push_interval{5};
	int			alarm_app_push{0};
	bool		server_push_enable{false};
	int			server_pic_type{0};
	bool		server_attr_enable{false};
	int			record_pic_type{0};
	bool		alarm_relay_enable{false};
	int			alarm_relay_interval{5};
	int			device_id{-1};	// -1 means local
	std::string model_path{"./models/person_20241206_npu1.model"};
	std::string task_id{"0"};

	region::RegionRule region_rule{};
	float region_track_timeout_sec{10.f};

	PersonLLMReviewConfig llm_review{};
};

class personDetectNode : public jdk_node_base, public CustomHandleFrame, public CustomHandleControl {
public:
	personDetectNode(std::string node_name, std::unique_ptr<PersonDetParams> params);
	~personDetectNode();
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
	void init_region_analyzer();
	std::pair<nlohmann::json, std::vector<std::function<std::shared_ptr<AXVideoFrame>()>>>
		 alarm_fn(const std::any& future_any, std::shared_ptr<AXVideoFrame> canvas);
	void render_fn(std::shared_ptr<AXVideoFrame>& canvas, const std::any& future_any, const std::any& extra = {});
	// std::string imageToBase64(std::shared_ptr<AXVideoFrame> frame);

	std::shared_ptr<HwIvps>			 ivps_		 = nullptr;
	std::shared_ptr<HwCapture>		 Capture_	 = nullptr;
	std::unique_ptr<PersonDetParams> nodeParams_ = nullptr;
	enum class PicType : int { FACE		  = 1 << 0,
							   BODY		  = 1 << 1,
							   BACKGROUND = 1 << 2 };
	enum class CapMode : int { SNAPSHOT_AFTER_LEAVING = 0,
							   REAL_TIME_SNAPSHOT	  = 1,
							   INTERVAL_SNAPSHOT	  = 2 };

	// axdl_sdk_t*										   infer_ = nullptr;
	// ax_algorithm_handle_t							   handle_det_ = nullptr;
	std::shared_ptr<SafeAlgorithm> alg_{nullptr};
	int							   device_id_ = -1;
	std::string					   task_id_{};
	int							   channel_id_;	 // default 0
	bool						   isHost() { return device_id_ < 0; }
	ax_image_t					   image_nv12_ = {0};
	// track
	struct TrackInfo {
		std::chrono::steady_clock::time_point last_ts;
		ax_bbox_t							  bbox{};
		std::shared_ptr<AXVideoFrame>		  face_snapshot{nullptr};
		std::shared_ptr<AXVideoFrame>		  body_snapshot{nullptr};
		std::shared_ptr<AXVideoFrame>		  bg_snapshot{nullptr};
		bool								  reported{false};
		bool								  departed{false};
	};
	std::unordered_map<unsigned long, TrackInfo> tracks_;
	std::vector<std::vector<cv::Point>>			 regions_{};
	region::RegionAnalyzer					 analyzer_;
	std::unordered_map<unsigned long, region::RegionEvent> pending_region_events_;
	std::deque<region::RegionEvent>		 pending_region_global_events_;
	std::mutex									 mutex_;
	MetricsReporter								 reporter_{5};
};
}  // namespace jdk_nodes
