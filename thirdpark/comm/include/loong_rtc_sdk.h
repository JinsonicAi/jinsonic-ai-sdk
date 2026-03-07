#ifndef _LOONG_RTC_SDK_H_
#define _LOONG_RTC_SDK_H_

#pragma once

#include <cstdint>

#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef BUILDING_DLL
        #define LOONG_RTC_SDK_API __declspec(dllexport)
    #else
        #define LOONG_RTC_SDK_API __declspec(dllimport)
    #endif
#else  // Linux/macOS
    #if __GNUC__ >= 4
        #define LOONG_RTC_SDK_API __attribute__((visibility("default")))
    #else
        #define LOONG_RTC_SDK_API
    #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////////

typedef enum {
    SDK_RTC_CODEC_H264 = 1,                                                   //!< H264 video codec
    SDK_RTC_CODEC_OPUS = 2,                                                   //!< OPUS audio codec
    SDK_RTC_CODEC_VP8 = 3,                                                    //!< VP8 video codec.
    SDK_RTC_CODEC_MULAW = 4,                                                  //!< MULAW audio codec
    SDK_RTC_CODEC_ALAW = 5,                                                   //!< ALAW audio codec
    SDK_RTC_CODEC_UNKNOWN = 6,
    SDK_RTC_CODEC_H265 = 7,                                                   //!< H265 video codec

    // SDK_RTC_CODEC_MAX **MUST** be the last enum in the list **ALWAYS** and not assigned a value
    SDK_RTC_CODEC_MAX //!< Placeholder for max number of supported codecs
} SDK_RTC_CODEC;

typedef enum {
    SDK_SESSION_CONTROL = 1,            //!< DataChannel only
    SDK_SESSION_MEDIA_PREVIEW = 2,      //!< One-way media
    SDK_SESSION_MEDIA_BIDIRECTIONAL = 3 //!< Two-way media
} SDK_SESSION_TYPE;

typedef struct __SdkSessionOptions {
    SDK_RTC_CODEC videoCodec; // use SDK_RTC_CODEC_UNKNOWN to keep default
    SDK_RTC_CODEC audioCodec; // use SDK_RTC_CODEC_UNKNOWN to keep default
    bool bidirectional;       // true for sendrecv
    const char* sessionId;    // optional custom session id (can be nullptr)
} SdkSessionOptions, *PSdkSessionOptions;

typedef struct __SdkConfig {
    bool sendOnly; // true: send only, false: receive only
    SDK_RTC_CODEC videoCodec; // video codec type
    SDK_RTC_CODEC audioCodec; // audio codec type

    char deviceId[32]; // device ID

    uint32_t frameWidth;  // frame width
    uint32_t frameHeight; // frame height

    // ============================================================
    // P2P UID + Token 鉴权（可选：向后兼容）
    //
    // 说明：
    // - 新版信令服务器在 login 时要求携带 p2p_uid（以及在开启鉴权时要求 token）。
    // - 为了尽量少改动现有 API，这里采用“在结构体尾部追加字段”的方式扩展。
    //
    // 约定（与 signaling-server-go/docs 一致）：
    // - 设备端：deviceId == p2pUid，role = "device"
    // - 客户端：deviceId 自己生成，p2pUid = 目标设备的 p2p_uid，role = "viewer"
    //
    // 注意：
    // - token 可能较长（JWT），请确保缓冲区足够大。
    // ============================================================
    char p2pUid[64];    // 对应登录 payload 的 p2p_uid（也兼容服务端字段 p2pUid）
    char token[1024];   // 授权服务器签发的 token（auth.enabled 且 allowLegacyNoToken=false 时必填）
    char role[16];      // "device" / "viewer"（可为空，由服务端兜底）
} SdkConfig, *PSdkConfig;

typedef struct __SdkFrame{
    // Id of the frame
    unsigned int index;
    // The decoding timestamp of the frame in 100ns precision
    unsigned long long decodingTs;
    // The presentation timestamp of the frame in 100ns precision
    unsigned long long presentationTs;
    unsigned int size;
    unsigned char *frameData;
} SdkFrame, *PSdkFrame;

typedef struct __SdkCallback {
    void (*OnVideoFrame)(const char* session_id, PSdkFrame frame);
    void (*OnAudioFrame)(const char* session_id, PSdkFrame frame);
    void (*OnDataChannelMessage)(const char* session_id, const char* label, const uint8_t* buffer, const uint32_t size);
    void (*OnSignalingMessage)(const char* session_id, const uint8_t* buffer, const uint32_t size, void* context);
} SdkCallback, *PSdkCallback;

///////////////////////////////////////////////////////////////////////////////////////////////

LOONG_RTC_SDK_API bool SdkInit(PSdkConfig config, PSdkCallback call_back, void* context);
LOONG_RTC_SDK_API void SdkDispose();

// P2P-first：设置 STUN-only 超时（毫秒），超时后回退 TURN 并重发 offer。
// - 推荐在 SdkInit 之前调用
// - ms=0 表示清除覆盖值，回退到默认/配置文件/环境变量
LOONG_RTC_SDK_API void SdkSetP2pFirstTimeoutMs(uint32_t ms);
// 返回当前生效的 P2P-first 超时（毫秒，已应用：接口覆盖 > env > 配置文件 > 默认值）
LOONG_RTC_SDK_API uint32_t SdkGetP2pFirstTimeoutMs();

LOONG_RTC_SDK_API void SdkConnectToPeer(const char* session_id);
LOONG_RTC_SDK_API void SdkConnectToPeerWithSession(const char* target_id, const char* session_id, bool bidirectional);
LOONG_RTC_SDK_API void SdkDisconnectToPeer(const char* session_id);
LOONG_RTC_SDK_API bool SdkSessionCreate(const char* target_id, SDK_SESSION_TYPE type, const SdkSessionOptions* options,
                                        char* out_session_id, uint32_t* inout_len);
LOONG_RTC_SDK_API void SdkSessionClose(const char* session_id);
LOONG_RTC_SDK_API void SdkSessionSend(const char* session_id, const char* channel, const uint8_t* buffer, const uint32_t size);

LOONG_RTC_SDK_API void SdkWriteVideoFrame(const char* session_id, PSdkFrame frame);
LOONG_RTC_SDK_API void SdkWriteVideoFrameToAll(PSdkFrame frame);  // 向所有活跃的视频通道发送视频帧
LOONG_RTC_SDK_API void SdkWriteAudioFrame(const char* session_id, PSdkFrame frame);
LOONG_RTC_SDK_API void SdkWriteAudioFrameToAll(PSdkFrame frame);  // 向所有活跃的视频通道发送音频帧

LOONG_RTC_SDK_API void SdkGetOnlineCount(uint32_t* count);
// Return a build/version tag string for the loaded libloongrtcsdk.so.
// out_tag will be NUL-terminated when max_len > 0.
// When arguments are invalid, this function is a no-op.
LOONG_RTC_SDK_API void SdkGetVersionTag(char* out_tag, uint32_t max_len);
LOONG_RTC_SDK_API void SdkSetFrameSize(const char* session_id, uint32_t width, uint32_t height);
LOONG_RTC_SDK_API void SdkGetConnectionType(const char* session_id, char* out_type, uint32_t max_len);
LOONG_RTC_SDK_API void SdkSetH265Sprop(const char* vps_b64, const char* sps_b64, const char* pps_b64);

LOONG_RTC_SDK_API void SdkSendCustomMessage(const char* session_id, const uint8_t* buffer, const uint32_t size);
LOONG_RTC_SDK_API void SdkSendFileTransferMessage(const char* session_id, const uint8_t* buffer, const uint32_t size);
LOONG_RTC_SDK_API void SdkSendCustomMessageToPeer(const char* session_id, const uint8_t* buffer, const uint32_t size);
LOONG_RTC_SDK_API void SdkSendFileTransferMessageToPeer(const char* session_id, const uint8_t* buffer, const uint32_t size);

LOONG_RTC_SDK_API void SdkSendSignalingMessage(const char* from, const char* to, const char* buffer);

///////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>

namespace loongrtc {
class Client;
class Session {
public:
    explicit Session(std::string id) : id_(std::move(id)) {}
    Session() = default;
    const std::string& id() const { return id_; }

    void Send(const char* channel, const uint8_t* buffer, uint32_t size) const {
        SdkSessionSend(id_.c_str(), channel, buffer, size);
    }
    void WriteVideo(PSdkFrame frame) const {
        SdkWriteVideoFrame(id_.c_str(), frame);
    }
    void WriteAudio(PSdkFrame frame) const {
        SdkWriteAudioFrame(id_.c_str(), frame);
    }
    void Close() const {
        SdkSessionClose(id_.c_str());
    }

private:
    std::string id_;
};

class Client {
public:
    explicit Client(std::string target_id) : target_id_(std::move(target_id)) {}
    explicit Client(const char* target_id) : target_id_(target_id ? target_id : "") {}

    Session CreateSession(SDK_SESSION_TYPE type, const SdkSessionOptions* options = nullptr) const {
        uint32_t len = 128;
        std::string buffer(len, '\0');
        bool ok = SdkSessionCreate(target_id_.c_str(), type, options, &buffer[0], &len);
        if (!ok && len > buffer.size()) {
            buffer.assign(len, '\0');
            ok = SdkSessionCreate(target_id_.c_str(), type, options, &buffer[0], &len);
        }
        if (!ok) {
            return Session("");
        }
        buffer.resize(len ? (len - 1) : 0);
        return Session(buffer);
    }

    bool CreateSession(SDK_SESSION_TYPE type, const SdkSessionOptions* options, Session* out) const {
        if (!out) return false;
        *out = CreateSession(type, options);
        return !out->id().empty();
    }

private:
    std::string target_id_;
};
} // namespace loongrtc
#endif

#endif // _LOONG_RTC_SDK_H_
