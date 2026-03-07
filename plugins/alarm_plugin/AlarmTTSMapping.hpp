#pragma once

#include <json.hpp>
#include <string>
#include <vector>

/**
 * @brief TTS播报数据结构 (完全解耦设计)
 * 
 * 使用方法：
 * 1. 各算法插件在 alarm_fn 返回的 JSON 中直接设置播报内容
 * 2. 可以设置 tts_text (文字转语音) 或 tts_url (播放音频URL)
 * 3. 报警插件不需要知道具体的报警类型，只负责调用音柱接口
 * 
 * 示例：
 * 
 * // 使用TTS文字转语音
 * result["tts_text"] = "警告！检测到有人抽烟，请立即停止！";
 * 
 * // 使用预录音频URL
 * result["tts_url"] = "http://192.168.1.100/audio/alarm_smoke.mp3";
 * 
 * // 或者在 local_push_msg 中设置
 * result["local_push_msg"]["tts_text"] = "警告！...";
 */
namespace AlarmTTS {

/**
 * @brief 单条TTS播报信息
 */
struct TTSItem {
	std::string text;       // TTS播报文本
	std::string url;        // 音频URL (优先于text)
	
	bool hasContent() const { return !url.empty() || !text.empty(); }
	bool isUrlMode() const  { return !url.empty(); }
};

/**
 * @brief TTS播报信息 (从报警JSON中提取，支持队列)
 */
struct TTSInfo {
	std::vector<TTSItem> items;  // 播报队列（多条依次播放）
	bool                 enabled{true};
	
	bool hasContent() const { return !items.empty(); }
};

/**
 * @brief 从报警JSON中提取TTS信息 (支持队列模式，多条依次播放)
 * 
 * 提取优先级:
 * 1. tts_queue 数组（多条依次播放）
 * 2. 单条 tts_url / tts_text
 * 3. alarms 数组中各项的播报内容
 * 
 * @param alarm_result 报警结果JSON (由算法插件的 alarm_fn 返回)
 * @return TTSInfo 包含播报队列
 */
inline TTSInfo extractTTSInfo(const nlohmann::json& alarm_result) {
	TTSInfo info{};
	info.enabled = true;
	
	// 检查是否显式禁用
	if (alarm_result.contains("tts_enabled")) {
		if (alarm_result["tts_enabled"].is_boolean()) {
			info.enabled = alarm_result["tts_enabled"].get<bool>();
		}
	}
	if (!info.enabled) return info;
	
	// 辅助函数：添加单条播报到队列
	auto addItem = [&](const std::string& text, const std::string& url) {
		TTSItem item;
		item.text = text;
		item.url = url;
		if (item.hasContent()) {
			info.items.push_back(item);
		}
	};
	
	// 辅助函数：从JSON对象提取播报内容
	auto extractFromJson = [&](const nlohmann::json& obj) {
		// 检查 tts_queue 数组（多条播报）
		if (obj.contains("tts_queue") && obj["tts_queue"].is_array()) {
			for (const auto& item : obj["tts_queue"]) {
				std::string url, text;
				if (item.contains("tts_url") && item["tts_url"].is_string()) {
					url = item["tts_url"].get<std::string>();
				}
				if (item.contains("tts_text") && item["tts_text"].is_string()) {
					text = item["tts_text"].get<std::string>();
				}
				addItem(text, url);
			}
			return true;
		}
		
		// 单条播报
		std::string url, text;
		if (obj.contains("tts_url") && obj["tts_url"].is_string()) {
			url = obj["tts_url"].get<std::string>();
		}
		if (obj.contains("tts_text") && obj["tts_text"].is_string()) {
			text = obj["tts_text"].get<std::string>();
		}
		if (!url.empty() || !text.empty()) {
			addItem(text, url);
			return true;
		}
		return false;
	};
	
	// 1. 检查根级别
	if (extractFromJson(alarm_result)) {
		return info;
	}
	
	// 2. 检查 alarms 数组
	if (alarm_result.contains("alarms") && alarm_result["alarms"].is_array()) {
		for (const auto& alarm : alarm_result["alarms"]) {
			// 检查 local_push_msg
			if (alarm.contains("local_push_msg") && alarm["local_push_msg"].is_object()) {
				extractFromJson(alarm["local_push_msg"]);
			}
			// 也直接检查 alarm 对象
			else {
				extractFromJson(alarm);
			}
		}
	}
	
	return info;
}

/**
 * @brief 检查报警是否需要TTS播报
 * 
 * @param alarm_result 报警结果JSON
 * @return true表示需要播报
 */
inline bool shouldTTSSpeak(const nlohmann::json& alarm_result) {
	// 检查是否显式禁用
	if (alarm_result.contains("tts_enabled")) {
		if (alarm_result["tts_enabled"].is_boolean() && !alarm_result["tts_enabled"].get<bool>()) {
			return false;
		}
	}
	
	// 检查是否有有效的报警
	int alarm_count = alarm_result.value("alarm_count", 0);
	return alarm_count > 0;
}

} // namespace AlarmTTS
