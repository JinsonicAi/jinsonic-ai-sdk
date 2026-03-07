#pragma once

#include <curl/curl.h>
#include <fmt/core.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>
#include <json.hpp>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

/**
 * @brief 音柱TTS播报器
 * 
 * 基于音柱HTTP接口实现语音播报功能，支持：
 * - 异步播报
 * - 队列模式（多条消息排队播放）
 * - 防抖机制（相同内容短时间内不重复播报）
 * - 循环播放
 * 
 * 接口文档参考：文本TTS与媒体URL播放接口.pdf
 */

/**
 * @brief TTS播报选项（定义在类外以兼容旧版GCC）
 */
struct TTSSpeakOptions {
	std::string vcn           {"xiaoyan"};  // 发音人: xiaoyan(女声), xiaofeng(男声)
	int         speed         {50};         // 语速 [0-100]
	int         volume        {50};         // 音量 [0-100]
	std::string rdn           {"2"};        // 数字发音: 0数值优先, 1完全数值, 2完全字符串, 3字符串优先
	std::string rcn           {"0"};        // 数字1的发音: 0=yao, 1=yi
	int         reg           {0};          // 英文发音: 0自动识别, 1逐个字母
	bool        sync          {false};      // 同步模式
	bool        queue         {true};       // 队列模式
	bool        prompt        {false};      // 播放前提示音
	// 循环播放选项
	int         loop_duration {0};          // 循环时长(秒), 0表示不循环
	int         loop_times    {0};          // 循环次数, 0表示不循环
	int         loop_gap      {2};          // 循环间歇(秒)
};

class TTSSpeaker {
public:
	using SpeakOptions = TTSSpeakOptions;

	/**
	 * @brief 构造函数
	 * @param speakerIp 音柱IP地址 (例如 "192.168.8.88")
	 * @param timeoutSeconds HTTP请求超时时间
	 */
	explicit TTSSpeaker(const std::string& speakerIp, long timeoutSeconds = 5)
		: speaker_ip_(speakerIp), timeout_(timeoutSeconds) {
		curl_global_init(CURL_GLOBAL_ALL);
		fmt::print("✅ TTSSpeaker initialized for speaker: {}\n", speakerIp);
	}

	~TTSSpeaker() {
		curl_global_cleanup();
	}

	/**
	 * @brief 设置音柱IP地址
	 */
	void setSpeakerIp(const std::string& ip) {
		std::lock_guard<std::mutex> lk(mutex_);
		speaker_ip_ = ip;
	}

	/**
	 * @brief 获取当前音柱IP
	 */
	std::string getSpeakerIp() const {
		std::lock_guard<std::mutex> lk(mutex_);
		return speaker_ip_;
	}

	/**
	 * @brief 检查音柱是否在线
	 * @return true表示在线，false表示离线
	 */
	bool checkAlive() {
		std::string url = buildUrl("/v1/check_alive");
		return httpGet(url);
	}

	/**
	 * @brief 异步播报文本（使用默认选项）
	 */
	void speakAsync(const std::string& text, int64_t debounce_ms = 5000) {
		SpeakOptions options;
		speakAsync(text, options, debounce_ms);
	}

	/**
	 * @brief 异步播报文本（带防抖）
	 * @param text 要播报的文本
	 * @param options 播报选项
	 * @param debounce_ms 防抖时间(毫秒)，相同文本在此时间内不重复播报
	 */
	void speakAsync(const std::string& text, 
					const SpeakOptions& options, 
					int64_t debounce_ms) {
		if (text.empty() || speaker_ip_.empty()) return;

		// 防抖检查
		if (!shouldSpeak(text, debounce_ms)) {
			fmt::print("🔇 TTS debounced: {}\n", text.substr(0, 30));
			return;
		}

		std::thread([this, text, options]() {
			nlohmann::json payload = buildTTSPayload(text, options);
			std::string url = buildUrl("/v1/speech");
			
			if (httpPost(url, payload.dump())) {
				fmt::print("🔊 TTS播报: {}\n", text);
			} else {
				fmt::print("❌ TTS播报失败: {}\n", text);
			}
		}).detach();
	}

	/**
	 * @brief 异步播放网络URL音频（使用默认选项）
	 */
	void playUrlAsync(const std::string& audioUrl, int64_t debounce_ms = 5000) {
		SpeakOptions options;
		playUrlAsync(audioUrl, options, debounce_ms);
	}

	/**
	 * @brief 异步播放网络URL音频（带防抖）
	 * @param audioUrl 音频URL地址
	 * @param options 播放选项
	 * @param debounce_ms 防抖时间(毫秒)，相同URL在此时间内不重复播放
	 */
	void playUrlAsync(const std::string& audioUrl, 
					  const SpeakOptions& options,
					  int64_t debounce_ms) {
		if (audioUrl.empty() || speaker_ip_.empty()) return;

		// 防抖检查
		if (!shouldSpeak(audioUrl, debounce_ms)) {
			fmt::print("🔇 URL播放防抖: {}\n", audioUrl.substr(0, 50));
			return;
		}

		std::thread([this, audioUrl, options]() {
			nlohmann::json payload = buildUrlPayload(audioUrl, options);
			std::string url = buildUrl("/v1/speech");
			
			if (httpPost(url, payload.dump())) {
				fmt::print("🔊 播放URL: {}\n", audioUrl);
			} else {
				fmt::print("❌ URL播放失败: {}\n", audioUrl);
			}
		}).detach();
	}

	/**
	 * @brief 取消当前播放
	 */
	void cancel() {
		std::thread([this]() {
			std::string url = buildUrl("/v1/speech");
			httpDelete(url);
			fmt::print("⏹️ TTS已取消\n");
		}).detach();
	}

	/**
	 * @brief 设置播放音量
	 * @param volume 音量值 [1-100]
	 */
	void setVolume(int volume) {
		volume = std::clamp(volume, 1, 100);
		std::thread([this, volume]() {
			nlohmann::json payload = {{"ao_volume", volume}};
			std::string url = buildUrl("/v1/volume");
			httpPatch(url, payload.dump());
		}).detach();
	}

	/**
	 * @brief 查询播放状态
	 * @return 播放状态JSON，失败返回空对象
	 */
	nlohmann::json getPlayStatus() {
		std::string url = buildUrl("/v1/play_status");
		std::string response;
		if (httpGetWithResponse(url, response)) {
			try {
				return nlohmann::json::parse(response);
			} catch (...) {}
		}
		return {};
	}

private:
	std::string buildUrl(const std::string& path) const {
		return fmt::format("http://{}{}", speaker_ip_, path);
	}

	nlohmann::json buildTTSPayload(const std::string& text, const SpeakOptions& opt) {
		nlohmann::json payload = {
			{"text",   text},
			{"vcn",    opt.vcn},
			{"speed",  opt.speed},
			{"volume", opt.volume},
			{"rdn",    opt.rdn},
			{"rcn",    opt.rcn},
			{"reg",    opt.reg},
			{"sync",   opt.sync},
			{"queue",  opt.queue},
			{"prompt", opt.prompt}
		};

		// 添加循环播放选项
		if (opt.loop_duration > 0 || opt.loop_times > 0) {
			nlohmann::json loop;
			if (opt.loop_duration > 0) loop["duration"] = opt.loop_duration;
			if (opt.loop_times > 0)    loop["times"]    = opt.loop_times;
			loop["gap"] = opt.loop_gap;
			payload["loop"] = loop;
		}

		return payload;
	}

	nlohmann::json buildUrlPayload(const std::string& audioUrl, const SpeakOptions& opt) {
		nlohmann::json payload = {
			{"url",    audioUrl},
			{"sync",   opt.sync},
			{"queue",  opt.queue},
			{"volume", opt.volume},
			{"prompt", opt.prompt}
		};

		if (opt.loop_duration > 0 || opt.loop_times > 0) {
			nlohmann::json loop;
			if (opt.loop_duration > 0) loop["duration"] = opt.loop_duration;
			if (opt.loop_times > 0)    loop["times"]    = opt.loop_times;
			loop["gap"] = opt.loop_gap;
			payload["loop"] = loop;
		}

		return payload;
	}

	/**
	 * @brief 防抖检查
	 */
	bool shouldSpeak(const std::string& text, int64_t debounce_ms) {
		std::lock_guard<std::mutex> lk(mutex_);
		auto now = std::chrono::steady_clock::now();
		auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			now.time_since_epoch()).count();

		auto& last_time = speak_history_[text];
		if (now_ms - last_time < debounce_ms) {
			return false;
		}
		last_time = now_ms;
		return true;
	}

	// HTTP请求辅助函数
	static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
		output->append(static_cast<char*>(contents), size * nmemb);
		return size * nmemb;
	}

	bool httpPost(const std::string& url, const std::string& payload) {
		CURL* curl = curl_easy_init();
		if (!curl) return false;

		struct curl_slist* headers = nullptr;
		headers = curl_slist_append(headers, "Content-Type: application/json; charset=UTF-8");

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);

		CURLcode res = curl_easy_perform(curl);
		
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);

		return res == CURLE_OK;
	}

	bool httpGet(const std::string& url) {
		CURL* curl = curl_easy_init();
		if (!curl) return false;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		return res == CURLE_OK;
	}

	bool httpGetWithResponse(const std::string& url, std::string& response) {
		CURL* curl = curl_easy_init();
		if (!curl) return false;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		return res == CURLE_OK;
	}

	bool httpDelete(const std::string& url) {
		CURL* curl = curl_easy_init();
		if (!curl) return false;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		return res == CURLE_OK;
	}

	bool httpPatch(const std::string& url, const std::string& payload) {
		CURL* curl = curl_easy_init();
		if (!curl) return false;

		struct curl_slist* headers = nullptr;
		headers = curl_slist_append(headers, "Content-Type: application/json");

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);

		CURLcode res = curl_easy_perform(curl);
		
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);

		return res == CURLE_OK;
	}

private:
	std::string speaker_ip_;
	long        timeout_;
	mutable std::mutex mutex_;
	std::unordered_map<std::string, int64_t> speak_history_;  // 防抖历史记录
};
