#pragma once

#include <algorithm>
#include <cstdlib>
#include <string>

#include "json.hpp"

namespace jdk_alarm_linkage {

struct TTSConfig {
	bool		enabled{false};
	std::string text{};
	std::string url{};

	bool hasContent() const { return !url.empty() || !text.empty(); }
	bool isUrlMode() const { return !url.empty(); }
};

struct Config {
	bool	  relay_enable{false};
	int		  relay_interval{5};
	TTSConfig tts{};
};

namespace detail {

inline bool parse_bool_string(std::string value, bool default_value) {
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	if (value == "1" || value == "true" || value == "yes" || value == "on") {
		return true;
	}
	if (value == "0" || value == "false" || value == "no" || value == "off") {
		return false;
	}
	return default_value;
}

inline bool get_json_bool(const nlohmann::json& json, const char* key, bool default_value) {
	auto it = json.find(key);
	if (it == json.end()) {
		return default_value;
	}
	if (it->is_boolean()) {
		return it->get<bool>();
	}
	if (it->is_number_integer()) {
		return it->get<int>() != 0;
	}
	if (it->is_string()) {
		return parse_bool_string(it->get<std::string>(), default_value);
	}
	return default_value;
}

inline int get_json_int(const nlohmann::json& json, const char* key, int default_value) {
	auto it = json.find(key);
	if (it == json.end()) {
		return default_value;
	}
	if (it->is_number_integer()) {
		return it->get<int>();
	}
	if (it->is_number()) {
		return static_cast<int>(it->get<double>());
	}
	if (it->is_string()) {
		char*		end = nullptr;
		const auto	text = it->get<std::string>();
		const long	value = std::strtol(text.c_str(), &end, 10);
		if (end != text.c_str()) {
			return static_cast<int>(value);
		}
	}
	return default_value;
}

inline std::string get_json_string(const nlohmann::json& json, const char* key, std::string default_value) {
	auto it = json.find(key);
	if (it == json.end()) {
		return default_value;
	}
	if (it->is_string()) {
		return it->get<std::string>();
	}
	if (it->is_object()) {
		if (auto zh = it->find("zh"); zh != it->end() && zh->is_string()) {
			return zh->get<std::string>();
		}
		if (auto en = it->find("en"); en != it->end() && en->is_string()) {
			return en->get<std::string>();
		}
	}
	return default_value;
}

inline bool has_key(const nlohmann::json& json, const char* key) {
	return json.find(key) != json.end();
}

}  // namespace detail

inline Config parse_config(
	const nlohmann::json& config,
	const std::string& default_tts_text = {},
	bool default_relay_enable = false,
	int default_relay_interval = 5) {
	Config out{};

	out.relay_enable = default_relay_enable;
	out.relay_interval = std::max(1, default_relay_interval);

	if (detail::has_key(config, "alarm_relay_enable")) {
		out.relay_enable = detail::get_json_bool(config, "alarm_relay_enable", out.relay_enable);
	}
	if (detail::has_key(config, "alarm_relay_interval")) {
		out.relay_interval = std::max(1, detail::get_json_int(config, "alarm_relay_interval", out.relay_interval));
	}

	out.tts.enabled = detail::get_json_bool(config, "tts_alarm_enabled", false);
	out.tts.text = detail::get_json_string(config, "tts_alarm_text", default_tts_text);
	out.tts.url = detail::get_json_string(config, "tts_alarm_url", "");

	if (detail::has_key(config, "tts_fire_enabled")) {
		out.tts.enabled = detail::get_json_bool(config, "tts_fire_enabled", out.tts.enabled);
	}
	if (detail::has_key(config, "tts_fire_text")) {
		out.tts.text = detail::get_json_string(config, "tts_fire_text", out.tts.text);
	}
	if (detail::has_key(config, "tts_fire_url")) {
		out.tts.url = detail::get_json_string(config, "tts_fire_url", out.tts.url);
	}

	return out;
}

inline bool relay_enabled(const Config& cfg) {
	return cfg.relay_enable;
}

inline int relay_interval_seconds(const Config& cfg) {
	return std::max(1, cfg.relay_interval);
}

inline void attach_tts(nlohmann::json& msg, const Config& cfg) {
	if (!cfg.tts.enabled || !cfg.tts.hasContent()) {
		return;
	}
	if (!msg.is_object()) {
		msg = nlohmann::json::object();
	}
	if (cfg.tts.isUrlMode()) {
		msg["tts_url"] = cfg.tts.url;
	} else {
		msg["tts_text"] = cfg.tts.text;
	}
}

inline void attach_relay(nlohmann::json& root, const Config& cfg) {
	if (!root.is_object()) {
		root = nlohmann::json::object();
	}
	root["alarm_relay"] = {
		{"enable", relay_enabled(cfg)},
		{"interval", relay_interval_seconds(cfg)}};
}

}  // namespace jdk_alarm_linkage
