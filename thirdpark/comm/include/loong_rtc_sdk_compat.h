#ifndef _LOONG_RTC_SDK_COMPAT_H_
#define _LOONG_RTC_SDK_COMPAT_H_

// Compatibility layer for legacy (old) loong_rtc_sdk APIs.
// The project historically depended on the old SDK header which exposed:
//   - SdkCallback::OnMessage
//   - SdkConfig::uid
//   - SdkSendMessage / SdkEnableOnlineServer / SdkAddDevice / SdkGetDeviceIds ...
//
// New SDK exposes different callbacks and config fields. This header provides
// missing legacy function declarations so the existing code can compile,
// while implementations live in `src/loong_rtc_sdk_compat.cpp`.

#include "loong_rtc_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

// Legacy APIs (removed in new SDK) - provided by project compatibility layer.
// void SdkEnableOnlineServer(bool enable);
bool SdkAddDevice(const char* uid);
bool SdkRemoveDevice(const char* uid);
void SdkGetDeviceIds(const char*** uids, int32_t* count);

// Set the current active RTC session id (for replying via DataChannel).
// Usually called from SDK callbacks when receiving a message.
void LoongRtcCompatSetActiveSessionId(const char* session_id);
bool LoongRtcCompatSendRawCustomMessage(const char* payload);
// Targeted, asynchronous custom-message delivery. This is used by event
// subscriptions so one viewer never receives another viewer's events.
bool LoongRtcCompatSendRawCustomMessageToSession(const char* session_id,
												  const char* payload,
												  bool high_priority);
bool LoongRtcCompatIsSessionAvailable(const char* session_id);

// 只读：当前是否存在"可达"的活动 session（有客户端连着且未被 ICE 清理）。
// 内部取当前 active session 并用 SdkIsSessionAvailable 判活；若已断开会顺带清空绑定。
// 上层可据此判断"是否真的有客户端在线"，作为主动推送/登录态的权威依据，
// 避免仅凭 is_login_ 逻辑标志（客户端异常断开时不会复位）而向死 session 空推。
bool LoongRtcCompatHasLiveSession(void);

// Stop compatibility-layer worker/session state before TaskManager tears down
// the RTC SDK. Safe to call repeatedly during an idempotent shutdown.
void LoongRtcCompatShutdown(void);

// Thread-local response interception used by non-RTC protocol adapters that
// must invoke the same legacy business handler as the official Web UI.  The
// callback returns true when it consumed the message; false preserves normal
// RTC delivery (important for PUSH_* notifications emitted by the handler).
// A capture is intentionally thread-local so concurrent viewers and OpenAPI
// requests cannot steal each other's replies.
typedef bool (*LoongRtcCompatMessageCaptureFn)(const char* type,
												const char* msg,
												int32_t code,
												const char* data,
												bool online,
												void* user_data);
bool LoongRtcCompatBeginThreadMessageCapture(LoongRtcCompatMessageCaptureFn callback, void* user_data);
void LoongRtcCompatEndThreadMessageCapture(void);

// 媒体回源: 把文件字节以二进制分帧回传到发起请求的 session（file-transfer 通道），
// 按 requestId 与前端挂起的 getMediaFile 请求关联。mime 为内容类型提示（可空）。
// 返回 false 表示当前无活动 session。
#ifdef __cplusplus
bool LoongRtcCompatSendMediaBytes(const char* requestId, const uint8_t* data, size_t size, const char* mime = nullptr,
								  size_t totalSize = 0, size_t rangeOffset = 0);
#else
bool LoongRtcCompatSendMediaBytes(const char* requestId, const uint8_t* data, size_t size, const char* mime,
								  size_t totalSize, size_t rangeOffset);
#endif

#ifdef __cplusplus
void SdkSendMessage(const char* type, const char* msg, int32_t code, const char* data, bool online = false);
void SdkSendBinaryMessage(const char* type, const char* msg, int32_t code, const uint8_t* data, const uint32_t size, bool online = false);
#else
void SdkSendMessage(const char* type, const char* msg, int32_t code, const char* data, bool online);
void SdkSendBinaryMessage(const char* type, const char* msg, int32_t code, const uint8_t* data, const uint32_t size, bool online);
#endif

#ifdef __cplusplus
}
#endif

#endif	// _LOONG_RTC_SDK_COMPAT_H_
