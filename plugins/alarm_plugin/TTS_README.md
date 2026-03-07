# 音柱TTS播报功能使用说明

## 概述

本功能基于音柱HTTP接口实现**完全解耦**的TTS语音播报。

**设计原则：**
- 每个算法插件独立管理自己的TTS配置
- 报警插件只负责调用音柱接口，不关心具体的报警类型
- 各插件之间无任何依赖关系

## 架构设计（完全解耦）

```
┌─────────────────────────────────────────────────────────────────────┐
│                   算法插件（各自独立管理TTS配置）                      │
├─────────────────────────────────┬───────────────────────────────────┤
│         firedet_plugin          │        personAttr_plugin          │
│                                 │                                   │
│ 配置项：                         │ 配置项（每种行为独立配置）：         │
│  ├─ tts_fire_enabled           │  ├─ tts_smoke_enabled/text/url    │
│  ├─ tts_fire_text              │  ├─ tts_no_hardhat_enabled/text/url│
│  └─ tts_fire_url               │  ├─ tts_no_safety_cloth_...       │
│                                 │  ├─ tts_facemask_...              │
│ 网页UI可配置！                    │  ├─ tts_cellphone_...             │
│                                 │  └─ tts_fighting_...              │
│                                 │                                   │
│                                 │ 网页UI每种行为可独立配置！           │
└────────────────┬────────────────┴──────────────────┬────────────────┘
                 │                                   │
                 │ JSON: tts_text 或 tts_url         │
                 ▼                                   ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         alarmNode (报警插件)                          │
│                                                                     │
│  只负责：提取 tts_text 或 tts_url → 调用 TTSSpeaker                   │
│  ❌ 不关心具体是什么类型的报警                                          │
│  ❌ 不维护任何报警类型配置                                              │
└──────────────────────────────────┬──────────────────────────────────┘
                                   │
                                   ▼
                             ┌─────────┐
                             │  音柱   │
                             └─────────┘
```

## firedet_plugin 配置说明

### 网页UI配置项

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| 火灾报警播报 | switch | false | 是否启用TTS播报 |
| 播报文字 | input | "警报！检测到烟雾或火焰..." | TTS播报内容 |
| 音频URL(优先) | input | "" | 预录音频URL，优先于文字 |

### 配置示例（JSON）

```json
{
  "tts_fire_enabled": true,
  "tts_fire_text": "警报！检测到烟雾或火焰，请注意安全并立即撤离！",
  "tts_fire_url": ""
}
```

或使用预录音频：

```json
{
  "tts_fire_enabled": true,
  "tts_fire_url": "http://192.168.1.100/audio/fire_alarm.mp3"
}
```

## personAttr_plugin 配置说明

### 支持的行为类型

每种行为类型都可以**独立配置**TTS播报：

| 行为类型 | 配置前缀 | 默认播报文字 |
|----------|----------|--------------|
| 抽烟 | tts_smoke_ | 注意！检测到有人抽烟，请立即停止吸烟！ |
| 未戴安全帽 | tts_no_hardhat_ | 注意！检测到有人未佩戴安全帽，请立即佩戴！ |
| 未穿反光衣 | tts_no_safety_cloth_ | 注意！检测到有人未穿反光衣，请立即穿戴！ |
| 口罩检测 | tts_facemask_ | 注意！检测到有人未佩戴口罩，请注意防护！ |
| 打电话 | tts_cellphone_ | 注意！检测到有人在使用手机，请放下手机！ |
| 打架 | tts_fighting_ | 警告！检测到打架行为，请立即停止！ |

### 每种行为的配置项

以"抽烟"为例：

| 配置项 | Key | 说明 |
|--------|-----|------|
| 启用播报 | tts_smoke_enabled | 是否启用该行为的TTS播报 |
| 播报文字 | tts_smoke_text | 文字转语音内容 |
| 音频URL | tts_smoke_url | 预录音频URL（优先于文字） |

### 配置示例（JSON）

只启用抽烟和安全帽的播报：

```json
{
  "tts_smoke_enabled": true,
  "tts_smoke_text": "注意！检测到有人抽烟，请立即停止吸烟！",
  "tts_smoke_url": "",
  
  "tts_no_hardhat_enabled": true,
  "tts_no_hardhat_text": "注意！检测到有人未佩戴安全帽，请立即佩戴！",
  "tts_no_hardhat_url": "",
  
  "tts_no_safety_cloth_enabled": false,
  "tts_facemask_enabled": false,
  "tts_cellphone_enabled": false,
  "tts_fighting_enabled": false
}
```

使用预录音频：

```json
{
  "tts_smoke_enabled": true,
  "tts_smoke_url": "http://192.168.1.100/audio/smoke_alarm.mp3",
  
  "tts_no_hardhat_enabled": true,
  "tts_no_hardhat_url": "http://192.168.1.100/audio/hardhat_alarm.mp3"
}
```

## 报警插件（alarm_plugin）配置

报警插件只需配置音柱的连接参数：

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| tts_enabled | bool | false | 是否启用TTS播报功能 |
| tts_speaker_ip | string | "" | 音柱IP地址 (如 192.168.8.88) |
| tts_volume | int | 50 | 播报音量 [1-100] |
| tts_speed | int | 50 | 语速 [0-100] |
| tts_vcn | string | "xiaoyan" | 发音人: xiaoyan(女声), xiaofeng(男声) |
| tts_debounce_ms | int | 5000 | 防抖时间(毫秒) |

```json
{
  "tts_enabled": true,
  "tts_speaker_ip": "192.168.8.88",
  "tts_volume": 60,
  "tts_speed": 50,
  "tts_vcn": "xiaoyan"
}
```

## 如何在其他算法插件中添加TTS功能

### 第1步：在头文件中定义配置结构

```cpp
// YourPlugin.hpp
struct TTSBroadcastConfig {
    bool        enabled{false};
    std::string text{};
    std::string url{};
    
    bool hasContent() const { return !url.empty() || !text.empty(); }
    bool isUrlMode() const  { return !url.empty(); }
};

struct YourNodeParams {
    // ... 其他参数
    TTSBroadcastConfig tts_alarm{};  // TTS配置
};
```

### 第2步：在配置模板中添加UI

```json
{
  "type": "switch",
  "name": "报警播报",
  "key": "tts_alarm_enabled",
  "default": false
},
{
  "type": "input",
  "name": "播报文字",
  "key": "tts_alarm_text",
  "default": "您的默认播报内容"
},
{
  "type": "input",
  "name": "音频URL(优先)",
  "key": "tts_alarm_url",
  "default": ""
}
```

### 第3步：在plugin_infer.cpp中读取配置

```cpp
nodeParams->tts_alarm.enabled = jp(config, "tts_alarm_enabled", false);
nodeParams->tts_alarm.text    = jp(config, "tts_alarm_text", std::string("默认内容"));
nodeParams->tts_alarm.url     = jp(config, "tts_alarm_url", std::string(""));
```

### 第4步：在alarm_fn中设置TTS内容

```cpp
// 检测到报警时
if (alarm_detected && nodeParams_->tts_alarm.enabled && nodeParams_->tts_alarm.hasContent()) {
    if (nodeParams_->tts_alarm.isUrlMode()) {
        result["local_push_msg"]["tts_url"] = nodeParams_->tts_alarm.url;
    } else {
        result["local_push_msg"]["tts_text"] = nodeParams_->tts_alarm.text;
    }
}
```

## 防抖机制

- 相同的播报内容在 `tts_debounce_ms` 时间内不会重复播报
- 这避免了同一报警场景的频繁语音提示
- URL播放和文字播报都有防抖机制

## 音柱接口参考

详见 `文本TTS与媒体URL播放接口(1).pdf`

### 支持的播放方式

| 方式 | JSON字段 | 说明 |
|------|----------|------|
| 文字转语音 | tts_text | 音柱将文字转换为语音播放 |
| 播放音频URL | tts_url | 直接播放网络音频（http/https/rtsp） |

**注意：** `tts_url` 优先于 `tts_text`，如果两者都设置，只播放URL音频。

## 编译说明

确保各插件的 CMakeLists.txt 中正确配置（无额外依赖）。
