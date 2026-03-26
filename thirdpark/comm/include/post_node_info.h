#ifndef _POST_NODE_INFO_H_
#define _POST_NODE_INFO_H_

// #pragma once

#include <cstdint>
#include <opencv2/core/core.hpp>
#define FMT_HEADER_ONLY
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>

#include "json.hpp"
#include "loong_rtc_sdk_compat.h"
#ifdef __cplusplus
extern "C" {
#endif

bool								isLogin();
bool								get_cloud_auth_info(std::string& token, std::string& tenant_id, int min_valid_sec);
void								update_cloud_auth_info(const std::string& token, const std::string& tenant_id, int expire_sec);
int									push_alarm_to_server(const std::string& path, const std::string& task_id, const std::string& task_name, const std::string& alarm_type, const std::string& msg);
std::vector<std::vector<cv::Point>> getRegions(const std::string task_id);

int insert_device_message_item(const char* did,
							   const char* name,
							   const char* type,
							   const char* content,
							   const char* img,
							   const char* date);

int insert_device_log_item(const char* did,
						   const char* name,
						   const char* type,
						   const char* content,
						   const char* date);

// delete device_message rows by image path and notify retrieval index removal.
// remove_file_if_exists: 0 only delete DB rows; non-zero also remove file from disk.
int delete_device_message_by_image_path(const char* img_path, int remove_file_if_exists);

// set rtsp decoding
// const char* id,  // device id
// const char* url,  // rtsp address
// const char* codec, // encoding format
// const char* resolution,  // resolution
// uint32_t fps // frame rate
void set_input_rtsp_info(const char* id, const char* url, const char* codec, const char* resolution, uint32_t fps);

// set algorithm detailed information
// const char* id,  // device id
// const char* algoid,  // device id
// const char* threshold,  // threshold
// const char* time, // inference time
// const char* delayed,  // delay
// uint32_t fps // frame rate
void set_algorithm_info(const char* id, const char* algoid, const char* threshold, const char* time, const char* delayed, uint32_t fps);

// set osd detailed information
// const char* id,  // device id
// const char* time, // inference time
// const char* delayed,  //delay
// uint32_t fps // frame rate
void set_osd_info(const char* id, const char* time, const char* delayed, uint32_t fps);

// set rtsp output details
// const char* id,  // device id
// const char* url,  // rtsp address
// const char* codec, // encoding format
// const char* resolution,  // resolution
// uint32_t fps // frame rate
void set_output_rtsp_info(const char* id, const char* url, const char* codec, const char* resolution, const char* delayed, uint32_t fps);

// generic node detail sync push:
// {
//   "id": "...",
//   "type": "...",
//   "detail_info": { ... }
// }
void set_node_detail_info(const char* id, const char* type, const char* detail_json);

// generic node form-status sync push:
// {
//   "id": "...",
//   "type": "...",
//   "form_status": { ... }
// }
void set_node_form_status(const char* id, const char* type, const char* form_status_json);

#ifdef __cplusplus
}
#endif

#endif	// _LOONG_RTC_SDK_H_
