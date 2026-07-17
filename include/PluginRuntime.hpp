#pragma once

#include <cctype>
#include <json.hpp>
#include <stdexcept>
#include <string>

#include "DeviceInfo.hpp"

struct PluginRuntime {
	std::string location;
	int         device_id{-1};
	int         runtime_device_id{-1};

	bool is_rk_local() const noexcept { return location == "rk.local"; }
	bool is_ax_local() const noexcept { return location == "ax.local"; }
	bool is_compute_card() const noexcept { return location.rfind("compute_card_", 0) == 0; }
	bool is_local() const noexcept { return is_rk_local() || is_ax_local(); }

	const char* infer_type() const noexcept {
		return is_rk_local() ? "rk" : "ax";
	}

	std::string model_soc() const {
		return model_soc_for(location, infer_type());
	}

	std::string pack_model_name(const std::string& base) const {
		return DeviceInfo::instance().packLogicalName(base, model_soc());
	}

	static std::string model_soc_for(std::string location, std::string infer_type) {
		location = normalize_location(std::move(location));
		for (char& ch : infer_type)
			ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		if (location == "rk.local" || infer_type == "rk") {
			return DeviceInfo::instance().targetSoc();
		}
		// Both AX local and AXCL compute cards use AX-platform models; do not select the
		// in-package logical name based on the RK/x86 host SoC.
		return "ax650";
	}

	static std::string pack_model_name(const std::string& base, const std::string& location, const std::string& infer_type) {
		return DeviceInfo::instance().packLogicalName(base, model_soc_for(location, infer_type));
	}

	static PluginRuntime from_task_config(const nlohmann::json& config) {
		PluginRuntime runtime;
		runtime.location = require_string(config, "runtime_location");
		runtime.device_id = require_int(config, "device_id");
		runtime.runtime_device_id = require_int(config, "runtime_device_id");
		runtime.location = normalize_location(std::move(runtime.location));

		if (runtime.is_local()) {
			if (runtime.device_id != -1 || runtime.runtime_device_id != -1) {
				throw std::invalid_argument("local runtime requires device_id and runtime_device_id to be -1");
			}
			return runtime;
		}

		if (runtime.is_compute_card() && runtime.runtime_device_id >= 0) {
			return runtime;
		}

		throw std::invalid_argument("unsupported runtime_location: " + runtime.location);
	}

private:
	static std::string require_string(const nlohmann::json& config, const char* key) {
		auto it = config.find(key);
		if (it == config.end() || !it->is_string() || it->get<std::string>().empty()) {
			throw std::invalid_argument(std::string("missing task runtime field: ") + key);
		}
		return it->get<std::string>();
	}

	static int require_int(const nlohmann::json& config, const char* key) {
		auto it = config.find(key);
		if (it == config.end() || !it->is_number_integer()) {
			throw std::invalid_argument(std::string("missing task runtime field: ") + key);
		}
		return it->get<int>();
	}

	static std::string normalize_location(std::string location) {
		while (!location.empty() && std::isspace(static_cast<unsigned char>(location.front())))
			location.erase(location.begin());
		while (!location.empty() && std::isspace(static_cast<unsigned char>(location.back())))
			location.pop_back();
		for (char& ch : location)
			ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		return location;
	}
};
