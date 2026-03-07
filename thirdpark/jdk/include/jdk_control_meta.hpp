#pragma once

#include <chrono>

#include "jdk_meta.hpp"

namespace jdk_objects {
// type of control meta
enum jdk_control_type {
	SPEAK,
	VIDEO_RECORD,
	IMAGE_RECORD,
	PIPELINE_DRAIN,
	CONFIG_ACTION
};

// control meta, which contains control data.
class jdk_control_meta : public jdk_meta {
private:
	// help to generate control uid if need
	void generate_uid();

public:
	jdk_control_meta(jdk_control_type control_type, int channel_index, std::string control_uid = "");
	~jdk_control_meta();

	jdk_control_type control_type;
	// unique id to identify control meta (caould be generated in random)
	std::string control_uid;

	// copy myself
	virtual std::shared_ptr<jdk_meta> clone() override;
};

}  // namespace jdk_objects
