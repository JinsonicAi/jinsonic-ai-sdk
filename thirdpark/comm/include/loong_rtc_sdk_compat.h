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
void SdkEnableOnlineServer(bool enable);
bool SdkAddDevice(const char* uid);
bool SdkRemoveDevice(const char* uid);
void SdkGetDeviceIds(const char*** uids, int32_t* count);

// Set the current active RTC session id (for replying via DataChannel).
// Usually called from SDK callbacks when receiving a message.
void LoongRtcCompatSetActiveSessionId(const char* session_id);

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

#endif // _LOONG_RTC_SDK_COMPAT_H_

