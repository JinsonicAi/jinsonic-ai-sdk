#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

#include "json.hpp"

namespace jdk_llm_review {

struct Config {
	bool		enabled{false};
	std::string prompt{};
	std::string expected_keyword{"YES"};
	int			timeout_ms{1200};
	bool		deny_on_mismatch{true};
	bool		follow_upstream_on_error{true};
	std::string decode_location{};
};

namespace detail {

template <typename T>
T get_json_value(const nlohmann::json& json, const char* key, T default_value) {
	auto it = json.find(key);
	if (it == json.end()) {
		return default_value;
	}
	try {
		return it->get<T>();
	} catch (...) {
		return default_value;
	}
}

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

}  // namespace detail

inline Config parse_config(
	const nlohmann::json& config,
	const std::string& default_prompt,
	int default_timeout_ms = 1200) {
	Config out{};
	out.enabled = detail::get_json_bool(config, "llm_review_enable", false);
	out.prompt = detail::get_json_string(config, "llm_review_prompt", default_prompt);
	out.expected_keyword = detail::get_json_string(config, "llm_review_expected_keyword", "YES");
	out.timeout_ms = std::max(200, detail::get_json_int(config, "llm_review_timeout_ms", default_timeout_ms));
	out.deny_on_mismatch = detail::get_json_bool(config, "llm_review_deny_on_mismatch", true);
	out.follow_upstream_on_error = detail::get_json_bool(config, "llm_review_follow_on_error", true);
	out.decode_location = detail::get_json_string(config, "llm_review_decode_location", "");
	return out;
}

inline nlohmann::json make_request(
	const Config& cfg,
	const std::string& node_name,
	const std::string& task_id,
	const std::string& alarm_type) {
	nlohmann::json req = {
		{"enabled", cfg.enabled},
		{"prompt", cfg.prompt},
		{"expected_keyword", cfg.expected_keyword},
		{"timeout_ms", cfg.timeout_ms},
		{"deny_on_mismatch", cfg.deny_on_mismatch},
		{"follow_upstream_on_error", cfg.follow_upstream_on_error},
		{"payload", {
			{"algo", node_name},
			{"task_id", task_id},
			{"alarm_type", alarm_type}
		}}
	};
	if (!cfg.decode_location.empty()) {
		req["decode_location"] = cfg.decode_location;
	}
	return req;
}

inline void attach_request(
	nlohmann::json& root,
	const Config& cfg,
	const std::string& node_name,
	const std::string& task_id,
	const std::string& fallback_alarm_type = {}) {
	if (!cfg.enabled) {
		return;
	}
	if (root.is_null() || root.empty()) {
		return;
	}
	if (auto it = root.find("alarm_count"); it != root.end()) {
		try {
			if (it->get<int>() <= 0) {
				return;
			}
		} catch (...) {
			return;
		}
	}
	std::string alarm_type = fallback_alarm_type;
	if (alarm_type.empty() && root.contains("alarm_type") && root["alarm_type"].is_string()) {
		alarm_type = root["alarm_type"].get<std::string>();
	}
	root["llm_request"] = make_request(cfg, node_name, task_id, alarm_type);
}

}  // namespace jdk_llm_review
