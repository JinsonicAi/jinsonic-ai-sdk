#pragma once

#include <optional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct RuntimeLocationResolution {
	std::string requested_location{"local"};
	int requested_device_id{-1};
	std::string runtime_location{"local"};
	int runtime_device_id{-1};
	bool fallback_applied{false};
	bool success{false};
	std::vector<int> available_device_ids;
	std::string error;
};

class EXPORT_VISIBILITY DeviceInfo {
public:
	static DeviceInfo& instance();
	const std::string& targetSoc() const noexcept;
	const std::string& deviceId() const noexcept;
	//
	const std::string& swVersion() const noexcept;
	const std::string& fwVersion() const noexcept;
	const std::string& OSVersion() const noexcept;

	bool supportLocal() const noexcept;	  //
	bool supportDevice() const noexcept;  //
	static std::string normalizeLocation(const std::string& raw, int fallback_device_id = -1);
	static int inferDeviceIdFromLocation(const std::string& raw, int fallback_device_id = -1);
	static std::string locationFromInferDeviceId(int infer_device_id);
	bool isInferDeviceIdAvailable(int infer_device_id) const noexcept;
	std::vector<int> availableInferDeviceIds(bool include_local = true) const;
	std::optional<int> resolveInferDeviceId(int requested_device_id, bool allow_fallback = true) const;
	RuntimeLocationResolution resolveRuntimeLocation(const std::string& requested_location,
													 int fallback_device_id = -1,
													 bool allow_fallback = true) const;
	RuntimeLocationResolution resolveRuntimeLocation(int requested_device_id,
													 bool allow_fallback = true) const;

	std::unordered_map<int, int> deviceId_map;	// Device ID Mapping Table，key: device_id, value: id index

private:
	DeviceInfo();
	~DeviceInfo() = default;

	DeviceInfo(const DeviceInfo&)			 = delete;
	DeviceInfo& operator=(const DeviceInfo&) = delete;

	std::string device_id_;
	std::string target_soc_;  // host name
	bool		support_local_;
	bool		support_device_{false};
	//
	mutable std::string	   swVersion_;	// deb version
	mutable std::string	   fwVersion_;	// 3.6.2
	mutable std::string	   osVersion_;	// system os version
	mutable std::once_flag osOnce_;
	mutable std::once_flag fwOnce_;
	mutable std::once_flag swOnce_;

	std::string getTargetSoc() const;
};

std::string run_axclsmi(int device_id, const std::string& shell_cmd, int timeout_seconds = 3);
