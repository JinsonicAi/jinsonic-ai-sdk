#pragma once

#include <atomic>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <string>

#include "HwCapture.hpp"
#include "HwIvps.hpp"
#include "JdkFireDetectNode.hpp"
#include "JdkOsd.hpp"
#include "MetricsReporter.hpp"
#include "alg_comm.hpp"
#include "ax_algorithm_sdk.h"
#include "jdk_node_base.hpp"
#include "jdk_node_wrapper.hpp"

namespace jdk_nodes {

/**
 * @brief TTS音柱播报配置（插件内独立定义，不依赖其他插件）
 */
struct TTSBroadcastConfig {
	bool        enabled{false};         // 是否启用TTS播报
	std::string text{};                 // 播报文字（TTS）
	std::string url{};                  // 音频URL（优先于text）
	
	bool hasContent() const { return !url.empty() || !text.empty(); }
	bool isUrlMode() const  { return !url.empty(); }
};

struct FireLLMReviewConfig {
	bool		enabled{false};
	std::string prompt{};
	std::string expected_keyword{"YES"};
	int			timeout_ms{1200};
	bool		deny_on_mismatch{true};
	bool		follow_upstream_on_error{true};
	std::string decode_location{};
};

struct FireNodeParams {
	float		threshold{0.8f};  // range: 0.0-1.0
	bool		show_fire{true};
	bool		alarm_push_enable{true};
	int			alarm_push_interval{5};
	int			alarm_app_push{0};
	int			record_pic_type{0};
	bool		alarm_relay_enable{false};
	int			alarm_relay_interval{5};
	int			device_id{-1};	// -1 means local
	std::string model_path{"./models/fs_20241231_npu1.model"};
	std::string task_id{"0"};
	
	// TTS音柱播报配置（火灾/烟雾报警）
	TTSBroadcastConfig tts_fire{};
	FireLLMReviewConfig llm_review{};
};

class fireDetectNode : public jdk_node_base, public CustomHandleFrame, public CustomHandleControl {
public:
	fireDetectNode(std::string node_name, std::unique_ptr<FireNodeParams> params);
	~fireDetectNode();
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
	void render_fn(std::shared_ptr<AXVideoFrame>& canvas, const std::any& future_any, const std::any& extra = {});
	jdk_osd::Overlay build_overlay_(const ax_result_t& det);
	// std::string imageToBase64(std::shared_ptr<AXVideoFrame> frame);

	std::shared_ptr<HwIvps>			ivps_		= nullptr;
	std::shared_ptr<HwCapture>		Capture_	= nullptr;
	std::unique_ptr<FireNodeParams> nodeParams_ = nullptr;
	enum class PicType : int { FIRE		  = 1 << 0,
							   BACKGROUND = 1 << 1,
	};

	// axdl_sdk_t*										   infer_ = nullptr;
	std::mutex mu_;
	// ax_algorithm_handle_t							   handle_	  = nullptr;
	int			device_id_ = -1;
	std::string task_id_{};
	int			channel_id_;  // default 0
	bool		isHost() { return device_id_ < 0; }
	ax_image_t	image_nv12_ = {0};
	// track
	const int track_timeout_sec_ = 10;
	struct TrackInfo {
		std::chrono::steady_clock::time_point last_ts;
		ax_bbox_t							  bbox{};
		std::shared_ptr<AXVideoFrame>		  fire_snapshot{nullptr};
		std::shared_ptr<AXVideoFrame>		  bg_snapshot{nullptr};
		bool								  reported{false};
	};
	std::unordered_map<unsigned long, TrackInfo> tracks_;
	std::vector<ax_bbox_t>						 alarms_;  // you need to call the police
	std::mutex									 mutex_;
	//
	// std::atomic<std::shared_ptr<SafeAlgorithm>> alg_{nullptr};
	std::shared_ptr<SafeAlgorithm> alg_{nullptr};
	MetricsReporter				   reporter_{5};
};
}  // namespace jdk_nodes
