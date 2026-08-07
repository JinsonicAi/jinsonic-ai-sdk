#pragma once

#include <cctype>
#include <filesystem>
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
		// AX 本地和 AXCL 计算卡都使用 AX 平台模型；不能按 RK/x86 宿主 SoC 选包内逻辑名。
		return "ax650";
	}

	static std::string pack_model_name(const std::string& base, const std::string& location, const std::string& infer_type) {
		return DeviceInfo::instance().packLogicalName(base, model_soc_for(location, infer_type));
	}

	// model_files is the package-to-runtime model contract. TaskManager replaces
	// package-relative entries with verified absolute cache paths before node creation.
	static std::string require_model_file(const nlohmann::json& config, size_t index = 0) {
		return require_resource_file(config, "model_files", index);
	}

	static std::string require_data_file(const nlohmann::json& config, size_t index = 0) {
		return require_resource_file(config, "data_files", index);
	}

	static std::string require_config_file(const nlohmann::json& config, size_t index = 0) {
		return require_resource_file(config, "config_files", index);
	}

	static std::string require_resource_file(const nlohmann::json& config,
										 const char* files_key, size_t index = 0) {
		const auto& files = require_resource_files(config, files_key);
		if (index >= files.size()) {
			throw std::invalid_argument(std::string(files_key) + " index is out of range: " +
				std::to_string(index));
		}
		return validate_resolved_resource_file(files[index], std::string(files_key) + "[" +
			std::to_string(index) + "]");
	}

	static std::string require_model_file_named(const nlohmann::json& config,
											 const std::string& filename) {
		return require_resource_file_named(config, "model_files", filename);
	}

	static std::string require_data_file_named(const nlohmann::json& config,
											const std::string& filename) {
		return require_resource_file_named(config, "data_files", filename);
	}

	static std::string require_config_file_named(const nlohmann::json& config,
											  const std::string& filename) {
		return require_resource_file_named(config, "config_files", filename);
	}

	static std::string require_resource_file_named(const nlohmann::json& config,
												const char* files_key,
												const std::string& filename) {
		if (filename.empty()) throw std::invalid_argument("resource filename is empty");
		const auto& files = require_resource_files(config, files_key);
		for (size_t index = 0; index < files.size(); ++index) {
			if (!files[index].is_string()) continue;
			const std::string path = files[index].get<std::string>();
			if (std::filesystem::path(path).filename() == filename) {
				return validate_resolved_resource_file(files[index], std::string(files_key) + "[" +
					std::to_string(index) + "]");
			}
		}
		throw std::invalid_argument(std::string("required ") + files_key +
			" entry is not declared: " + filename);
	}

	static std::string require_model_file_with_extension(const nlohmann::json& config,
												 const std::string& extension) {
		if (extension.empty()) throw std::invalid_argument("model extension is empty");
		const auto& files = require_resource_files(config, "model_files");
		for (size_t index = 0; index < files.size(); ++index) {
			if (!files[index].is_string()) continue;
			const std::string path = files[index].get<std::string>();
			if (std::filesystem::path(path).extension() == extension) {
				return validate_resolved_resource_file(files[index], "model_files[" + std::to_string(index) + "]");
			}
		}
		throw std::invalid_argument("required model extension is not declared: " + extension);
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
	static const nlohmann::json& require_resource_files(const nlohmann::json& config,
												 const char* files_key) {
		auto it = config.find(files_key);
		if (it == config.end() || !it->is_array() || it->empty()) {
			throw std::invalid_argument(std::string("missing non-empty task ") + files_key + " array");
		}
		return *it;
	}

	static std::string validate_resolved_resource_file(const nlohmann::json& value,
											  const std::string& label) {
		if (!value.is_string() || value.get_ref<const std::string&>().empty()) {
			throw std::invalid_argument(label + " must be a non-empty string");
		}
		const std::string path = value.get<std::string>();
		std::error_code ec;
		if (!std::filesystem::path(path).is_absolute() ||
			!std::filesystem::is_regular_file(path, ec) || ec) {
			throw std::invalid_argument(label + " is not a resolved model file: " + path);
		}
		return path;
	}

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
