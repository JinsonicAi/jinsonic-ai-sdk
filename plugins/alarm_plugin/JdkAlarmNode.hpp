#pragma once

#include <mutex>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <string>

#include "AlarmPusher.hpp"
#include "AlarmTTSMapping.hpp"
#include "HwCapture.hpp"
#include "MetricsReporter.hpp"
#include "TTSSpeaker.hpp"
#include "jdk_node_base.hpp"
#include "jdk_node_wrapper.hpp"

namespace jdk_nodes {

/**
 * @brief TTS音柱播报配置
 */
struct TTSConfig {
	bool        enabled       = false;           // 是否启用TTS播报
	std::string speaker_ip    = "";              // 音柱IP地址
	int         volume        = 50;              // 播报音量 [1-100]
	int         speed         = 50;              // 语速 [0-100]
	std::string vcn           = "xiaoyan";       // 发音人: xiaoyan(女), xiaofeng(男)
	int         debounce_ms   = 5000;            // 防抖时间(毫秒)
	uint32_t    behavior_type = 0xFFFFFFFF;      // 需要播报的行为类型掩码(默认全部)
};

class alarmNode : public jdk_node_base, public CustomHandleFrame, public CustomHandleControl {
public:
	alarmNode(std::string node_name, int device_id, std::string server_url, 
			  int interval_sec = 3, std::string task_id = "", std::string task_name = "",
			  TTSConfig tts_config = {});
	~alarmNode();
	void stop();
	
	/**
	 * @brief 设置TTS配置
	 */
	void setTTSConfig(const TTSConfig& config);
	
	/**
	 * @brief 手动触发TTS播报（供其他模块调用）
	 * @param text 播报内容
	 */
	void speakTTS(const std::string& text);

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
	void reportDeviceInfo();
	void sendPushMessages(nlohmann::json& alarm_result);
	void cleanupMessages(nlohmann::json& alarm_result);
	void processTTSAlarm(const nlohmann::json& alarm_result);

	std::shared_ptr<HwCapture>	 Capture_	   = nullptr;
	std::shared_ptr<AlarmPusher> ServerPusher_ = nullptr;
	std::shared_ptr<TTSSpeaker>  TTSSpeaker_   = nullptr;  // 音柱TTS播报器
	int							 device_id_	   = -1;
	std::string					 task_id_{};
	std::string					 task_name_{};
	std::string					 http_server_url_;
	TTSConfig                    tts_config_{};            // TTS配置
	//
	struct PushState {
		int64_t last_push_time_ms = 0;
	};

	std::unordered_map<std::string, PushState> push_states_;
	std::mutex								   mutex_;
	MetricsReporter							   reporter_;
};
}  // namespace jdk_nodes