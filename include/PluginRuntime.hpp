#pragma once

#include <cctype>
#include <json.hpp>
#include <stdexcept>
#include <string>

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
