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

bool isLogin();
bool get_cloud_auth_info(std::string& token, std::string& tenant_id, int min_valid_sec);
void update_cloud_auth_info(const std::string& token, const std::string& tenant_id, int expire_sec);
// int									push_alarm_to_server(const std::string& path, const std::string& task_id, const std::string& task_name, const std::string& alarm_type, const std::string& msg);
std::vector<std::vector<cv::Point>> getRegions(const std::string task_id);
std::vector<std::vector<cv::Point>> getRegionShapes(const std::string task_id);
std::string							get_protocol_runtime_config_json(const std::string& key);

// P2P transport APIs — called by p2p_plugin to send video frames.
// P2PManager (in TaskManager) handles session management, subscription, and PPCS SDK.
// The plugin node only needs to call p2p_send_video_frame() per encoded frame.
void p2p_send_video_frame(const std::string& task_id, bool is_keyframe, const uint8_t* data, size_t size, uint32_t timestamp);
void p2p_send_audio_frame(const uint8_t* data, size_t size, uint32_t timestamp);
bool p2p_is_initialized();
int	 p2p_get_active_session_count();

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

enum MediaEventTypeC {
	MEDIA_EVENT_UNKNOWN = 0,
	MEDIA_EVENT_ALARM_IMAGE_UPSERT = 1,
	MEDIA_EVENT_ALARM_IMAGE_DELETE = 2,
	MEDIA_EVENT_VIDEO_FILE_CLOSED = 3,
	MEDIA_EVENT_VIDEO_FILE_DELETED = 4,
};

struct MediaEventC {
	int type;
	const char* task_id;
	int64_t message_id;
	const char* file_path;
	const char* alarm_type;
	int64_t updated_at_ms;
	const char* source;
};

typedef void (*MediaEventCallback)(const MediaEventC* event, void* user_data);

// Subscribe/publish media lifecycle events asynchronously via TaskManager's MediaEventBus.
// Return value > 0 is a subscription id; pass it to unsubscribe_media_event().
// Callback is invoked on the MediaEventBus worker thread; keep it lightweight.
// The const char* fields in MediaEventC are valid only during the callback.
int subscribe_media_event(MediaEventCallback callback, void* user_data);
void unsubscribe_media_event(int subscription_id);
int publish_media_event(const MediaEventC* event);

// Task media context registry/query APIs.
// netserver-like output plugins should register their runtime RTSP output when it is available.
// Consumers can query by task_id; return value is the bytes required including trailing '\0'.
// If buf is null or buf_size is too small, the required size is still returned and buf is untouched/truncated safely.
void register_task_rtsp_output(const char* task_id, const char* node_id, const char* url, const char* codec, int rtsp_port);
void unregister_task_rtsp_output(const char* task_id, const char* node_id);
int get_task_media_context_json(const char* task_id, char* buf, int buf_size);

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