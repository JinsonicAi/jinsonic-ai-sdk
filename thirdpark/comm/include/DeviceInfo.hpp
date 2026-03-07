#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

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
