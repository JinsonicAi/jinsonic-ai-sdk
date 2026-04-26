<div align="left">
<a href="README_CN.md"><button>中文</button></a>
</div>

# AIBox SDK Customer Development Guide 

**Scope**
- SDK: `aibox_sdk` (this repository)
- Firmware baseline: `3.10.2`
- Integration protocol baseline: `Platform Device Integration Protocol Specification V1.0.2` (released on `2026-02-21`)
- Target hardware: AX650N / AX8850 (dev boards and compute cards)
- Target audience: algorithm engineers, backend engineers, delivery/integration engineers

---

## Table of Contents
1. [SDK Overview](#1-sdk-overview)
2. [Repository Structure](#2-repository-structure)
3. [Plugin Development and Fast Algorithm Integration (Key)](#3-plugin-development-and-fast-algorithm-integration-key)
4. [Alarm Protocol and Message Specification (Key)](#4-alarm-protocol-and-message-specification-key)
5. [SDK Demo Guide](#5-sdk-demo-guide)
6. [How to Use Plugin Configuration Files](#6-how-to-use-plugin-configuration-files)
7. [Key SDK Interfaces](#7-key-sdk-interfaces)
8. [Build, Deployment, and Runtime](#8-build-deployment-and-runtime)
9. [Troubleshooting](#9-troubleshooting)
10. [Additional Engineering Recommendations](#10-additional-engineering-recommendations)

---

## 1. SDK Overview

### 1.1 Core Design
- **Node**: the smallest processing unit in the pipeline.
- **Pipeline**: a DAG built with `attach_to()`.
- **Plugin**: dynamically loaded `.plugin` package (`data + config.json`).
- **SDKInterface**: runtime callbacks exposed to plugins (node registration, logging, config, events).

### 1.2 Typical Data Flow
`Input plugin (netclient/wwaCam)` -> `Algorithm plugin (persondet/firedet/...)` -> `Output plugin (alarm/record/gb28181/p2p/netserver/hdmi)`

### 1.3 Alarm Pipeline (High-Frequency Integration Path)
- Algorithm nodes write `ResultEntry` into `result_map_`.
- `ResultEntry` includes: `result + build_overlay + push policy`.
- `alarm_plugin` consumes `alarm_fn` outputs and performs unified routing:
  - local notification (UI/DB)
  - HTTP server push
  - event reporting via `/api/v1/device/report/event`
  - TTS speaker playback

---

## 2. Repository Structure

```text
aibox_sdk/
├── include/                       # Public SDK headers (required for plugin development)
│   ├── sdk_interface.hpp
│   ├── node_factory.hpp
│   ├── plugin_loader.hpp
│   ├── alg_comm.hpp
│   └── ax_algorithm_sdk.h
├── plugins/                       # Plugin implementations (input/algorithm/output)
│   ├── netclient_plugin/
│   ├── persondet_plugin/
│   ├── firedet_plugin/
│   ├── alarm_plugin/
│   └── ...
├── example/                       # SDK sample programs
├── doc/
│   ├── PLUGIN_CONFIG_REFERENCE_EN.md
│   └── 平台设备对接协议规范-V1.0.2.docx
├── build_sdk_sample.sh            # One-click build for samples and plugins
└── CMakeLists.txt
```

---

## 3. Plugin Development and Fast Algorithm Integration (Key)

## 3.1 Plugin Lifecycle (Must Know)

Every plugin must export two C functions:

```cpp
extern "C" void plugin_init(SDKInterface* sdk);
extern "C" void plugin_cleanup(SDKInterface* sdk);
```

Runtime flow:
1. `PluginLoader` scans `.plugin` files.
2. `plugin_init` calls `sdk->register_node(type, creator)`.
3. On unload, `plugin_cleanup` is called and `sdk->unregister_node(type)` runs.

## 3.2 Standard Plugin Development Procedure

### Step 1: Clone the Closest Existing Plugin Template
Recommended starting points: `persondet_plugin`, `firedet_plugin`, `netclient_plugin`.

> Directory naming recommendation: use `xxx_plugin` suffix. `plugins/CMakeLists.txt` auto-discovers subprojects using the `*_plugin` pattern.

### Step 2: Define Node Parameter Struct
Define `NodeParams` in `JdkXXXNode.hpp` and keep defaults centralized.

### Step 3: Parse Config in `plugin_infer.cpp`
Use `jp(config, key, default)` and register node:

```cpp
sdk->register_node(PLUGIN_NODE_NAME, [](const std::string& name, const nlohmann::json& config) {
    auto nodeParams = std::make_unique<MyNodeParams>();
    nodeParams->threshold = jp(config, "threshold", 0.8f);
    nodeParams->device_id = jp(config, "device_id", -1);
    nodeParams->model_path = jp(config, "model_path", "./models/xxx.model");

    return jdk_nodes::jdk_node_wrapper::create(
        name,
        std::make_shared<jdk_nodes::MyNode>(name, std::move(nodeParams)));
});
```

### Step 4: Implement Node Class (Algorithm Core)
Recommended inheritance:
- `jdk_node_base`
- `CustomHandleFrame`
- `CustomHandleControl`

Minimum implementations:
- `handle_frame_meta(std::shared_ptr<jdk_frame_meta>)`
- `handle_control_meta(...)`
- optional `run_infer_combinations(...)`

### Step 5: Integrate Inference Logic
For AX algorithm SDK integration, prefer `SafeAlgorithm`:

```cpp
SafeAlgorithm::Options opt{ax_model_type_fire_smoke, nodeParams_->model_path, nodeParams_->device_id};
alg_ = std::make_shared<SafeAlgorithm>(opt);
alg_->set_affinity(true);
alg_->update_params([&](auto &p) {
    p.det_threshold = 0.8f;
});
```

Common APIs:
- `detect(frame, result, capture)`
- `track(frame, result, capture)`
- `get_body_attr(...)`
- `get_plate(...)`
- `get_face_feature_2(...)`

### Step 6: Write Inference Result to `result_map_`
Create `jdk_objects::ResultEntry` with:
- `result`: inference result object (`std::any` in most plugins)
- `render_fn`: OSD callback
- `alarm_fn`: alarm payload callback
- `push_enabled/push_interval_ms`: push strategy

### Step 7: Return Standard Alarm JSON in `alarm_fn`
Strongly recommended: use the unified structure in Chapter 4. Otherwise `alarm_plugin` may fail to route HTTP/TTS/storage correctly.

### Step 8: Define `config_template.json.in`
- define component group: `input-component / algorithm-component / output-component / other-component`
- define UI form schema: `component.formList`

### Step 9: Build and Package `.plugin`
`plugins/CMakeLists.txt` auto-traverses `*_plugin` directories and packages each plugin.

### Step 10: Deploy and Validate
- Copy `.plugin` to device plugin directory.
- Restart service and verify node registration.
- Create task in web UI and validate end-to-end behavior.

## 3.3 Fast Integration of Your Own Algorithm

### Path A: Replace Inference Core in Existing Plugin (Recommended)
Best when input/output contract is similar.

Execution strategy:
1. Copy existing plugin (e.g., `persondet_plugin`).
2. Keep `render_fn`, `alarm_fn`, and `config_template`.
3. Replace only inference logic in `run_infer_combinations()`.
4. Keep `alarm_fn` field contract stable.

Pros:
- fastest delivery
- highest compatibility with existing UI/backend contracts

### Path B: Create a New Plugin Type
Best for completely new tasks.

Must align:
- new node `type` (registration name)
- new `config_template`
- new `alarm_type` and `EventType` mapping
- backend parser for new alarm type

## 3.4 Recommended Pipeline Templates

### Person Detection
`netclient_plugin -> persondet_plugin -> alarm_plugin`

### Face Recognition
`netclient_plugin -> facerec_plugin -> alarm_plugin (+ record_plugin optional)`

### Vehicle Capture
`netclient_plugin -> lpr_plugin -> alarm_plugin/gb28181_plugin`

## 3.5 Development Notes
- Node `type` must match `config.json.component.type`.
- `plugin_init/plugin_cleanup` must be exported correctly, otherwise: `undefined symbol: plugin_init`.
- Empty object from `alarm_fn` means no alarm processing in `alarm_plugin`.
- `push_interval_ms` is enforced at `ResultEntry` granularity.

---

## 4. Alarm Protocol and Message Specification (Key)

Two layers are involved:
- **Layer A (intra-SDK payload contract)**: between algorithm plugins and output plugins.
- **Layer B (device-to-platform protocol)**: HTTP reporting from device to platform.

## 4.1 Layer A: Unified Alarm JSON Structure

Recommended `alarm_fn` return schema:

```json
{
  "msg": "TaskId:xxx ...",
  "alarm_type": "persondet_alarm",
  "timestamp": "2026-03-07T10:10:10.123Z",
  "alarm_count": 1,
  "alarm_push_uri": "/api/v1/device/report/event",
  "server_push_uri": "/api/v1/face/capture",
  "alarms": [
    {
      "local_push_msg": {
        "alarm_type": "persondet_alarm",
        "bbox": {"x": 100, "y": 120, "w": 200, "h": 260},
        "tts_text": "Warning: abnormal behavior detected"
      },
      "server_push_msg": {
        "time": 1770000000000,
        "sn": "device-sn",
        "data": {
          "face_data": "...base64...",
          "bg_data": "...base64..."
        }
      },
      "alarm_push_msg": {
        "did": "device-id",
        "type": 11,
        "time": 1770000000000,
        "state": 0,
        "data": {
          "bg_data": "...base64..."
        }
      }
    }
  ]
}
```

`alarm_plugin` routing logic:
1. Read root-level `server_push_uri/alarm_push_uri`.
2. Iterate `alarms[]`.
3. If `server_push_msg` exists, POST to `server_push_uri`.
4. If `alarm_push_msg` exists, POST to `alarm_push_uri`.
5. `local_push_msg` is used for local notifications and TTS extraction, not direct external push.

## 4.2 Layer A Field Constraints (Recommended)

### Root fields
- `alarm_type`: alarm category string.
- `timestamp`: ISO8601 string.
- `alarm_count`: integer.
- `alarms`: array.
- `alarm_push_uri/server_push_uri`: optional; empty means disabled for that route.

### Per-alarm fields
- `local_push_msg`: local display / DB / TTS payload carrier.
- `server_push_msg`: structured business push (face/vehicle capture etc.).
- `alarm_push_msg`: standardized event push (`did/type/time/state/data`).

### TTS fields (local)
Can be placed in `local_push_msg`:
- `tts_text`
- `tts_url`
- `tts_queue` (array for queued playback)

## 4.3 `EventType` Mapping (Code Baseline)

Source: `thirdpark/comm/include/DevProtoDef.hpp`

| Value | Meaning |
|---|---|
| 1 | MOTION_DETECTION |
| 2 | INTRUSION_DETECTION |
| 3 | VIDEO_BLIND |
| 4 | BABY_CRYING |
| 5 | CROWD_GATHERING |
| 6 | PERSON_LOITERING |
| 7 | FAST_MOVING |
| 8 | FIRE_ALARM |
| 9 | SOUND_SPIKE |
| 10 | NOISE_DROP |
| 11 | HUMAN_DETECTION |
| 12 | WHITELIST_DETECTION |
| 13 | BLACKLIST_DETECTION |
| 14 | VIP_DETECTION |
| 15 | POST_ABANDONMENT |
| 16 | FALL_DETECTION |
| 17 | VITAL_SIGN_DETECTION |
| 18 | KITCHEN_MONITORING |
| 19 | BEHAVIOR_DETECTION |
| 20 | NON_MOTORIZED_VEHICLE |

## 4.4 Layer B: Device External HTTP Protocol

According to `Platform Device Integration Protocol Specification V1.0.2`:
- Event report: `POST /api/v1/device/report/event`
- Device info report: `POST /api/v1/device/report/info`
- Face capture upload: `POST /api/v1/adapter/lenfocus/face/capture`
- Vehicle capture upload: `POST /api/v1/adapter/lenfocus/vehicle/capture`

> Note: some default `server_push_uri` values in plugin code (for example `/api/v1/face/capture`) may differ from protocol doc paths because of gateway routing layers. Align with the actual platform gateway mapping in your deployment.

### Standard Body for `/api/v1/device/report/event`

```json
{
  "did": "1234512",
  "type": 1,
  "time": 1666781577816,
  "data": {
    "face_data": "data:image/jpeg;base64,...",
    "body_data": "data:image/jpeg;base64,...",
    "bg_data": "data:image/jpeg;base64,...",
    "jpeg_url_face": "sdcard/...jpg",
    "jpeg_url_body": "sdcard/...jpg",
    "jpeg_url_frame": "sdcard/...jpg"
  }
}
```

## 4.5 Protocol Version Compatibility (Important)

From code vs protocol doc comparison:
- protocol doc V1.0.2 examples mainly cover event values up to `14`
- SDK code currently extends `EventType` to `20` (e.g., `19/20`)

Recommendations:
1. backend enum parser should be forward-compatible (do not hard-reject unknown values)
2. classify using both `type` and `alarm_type`

## 4.6 Alarm Link Joint-Debug Checklist
1. verify algorithm plugin returns `alarm_count > 0`
2. verify `alarm_push_uri/server_push_uri` are non-empty when expected
3. verify `alarm_plugin.http_url` reachability
4. verify `did/type/time` fields are present
5. check image payload size (`data.bg_data`) for timeout risk

---

## 5. SDK Demo Guide

`example/` programs and their purpose:

| Demo | Purpose | Key Classes |
|---|---|---|
| `jdk_frame_sample` | frame object load/save | `AXVideoFrame` |
| `jdk_capture_sample` | NV12/JPEG capture | `HwCapture` |
| `jdk_ivps_sample` | IVPS processing (dewarp/CSC) | `HwIvps` |
| `jdk_npu_sample` | NPU inference + rendering | `YOLOFACE` + `HwIvps` |
| `jdk_alg_sample` | unified `SafeAlgorithm` usage (e.g., LPR) | `SafeAlgorithm` |
| `jdk_netclient_vdec_sample` | RTSP pull and frame fetch | `NetClient` |
| `jdk_netserver_venc_sample` | encoding/streaming sample | `HwEncoder` |
| `jdk_node_sample` | dynamic plugin loading and component-structure dump | `PluginLoader` + `NodeFactory` |

> `jdk_venc_vdec_vo_sample/readme.txt`: for encode/decode/display details, refer to plugin implementations.

### Recommended Validation Order
1. `jdk_frame_sample` -> verify I/O basics
2. `jdk_capture_sample` / `jdk_ivps_sample` -> verify image processing chain
3. `jdk_npu_sample` / `jdk_alg_sample` -> verify inference stack
4. `jdk_node_sample` + plugins -> verify plugin loading and UI schema output

---

## 6. How to Use Plugin Configuration Files

## 6.1 Configuration Layers

### A. Plugin package metadata (`config.json` top-level)
- `name/version/platform/entry/md5/type`
- `model_path/model_files`
- `component`

### B. Frontend component schema (`component`)
- `parentType`: `input-component / algorithm-component / output-component / other-component`
- `type`: node type (must match registration name)
- `label`: display name
- `component.formList`: form schema

## 6.2 Common `formList` Controls
See `doc/PLUGIN_CONFIG_REFERENCE_EN.md`

Core supported types:
- `input` / `inputNumber` / `password` / `textarea`
- `select` / `switch` / `slider`
- `button` / `divider` / `subdivider`
- `schedule` / `regionDraw`
- `readOnly` / `status`

## 6.3 Typical Configuration Patterns

### Input plugins
- `rtsp_url`
- `schedule_config`

### Algorithm plugins
- threshold, model path, push strategy, picture-type bitmask
- optional: `llm_review_*`, `tts_*`

### Output plugins
- `http_url`
- `tts_enabled/tts_speaker_ip/tts_volume/...`

## 6.4 Change Management Rules
1. new fields should always have defaults
2. avoid renaming old fields unless strictly necessary
3. any backend-parsed field changes require coordinated version update

---

## 7. Key SDK Interfaces

## 7.1 `SDKInterface` (Plugin Runtime Capabilities)

```cpp
struct SDKInterface {
    std::function<void(const std::string&, NodeCreator)> register_node;
    std::function<void(const std::string&)> unregister_node;
    std::function<void(const std::string& level, const std::string& message)> log;
    std::function<std::string(const std::string& key)> get_config;
    std::function<void(const std::string& topic, const std::string& payload)> publish_event;
};
```

## 7.2 `jdk_node_base` (Node Base Class)
Common APIs:
- `start()/stop()`
- `attach_to()/detach_recursively()`
- `make_frame_meta()/out_queue_push()`
- `is_alive()/set_alive()`

## 7.3 `jdk_frame_meta::result_map_`
`result_map_` is the result exchange bus:
`unordered_map<string, AtomicPtr<ResultEntry>>`

`ResultEntry` includes:
- `result`: inference output
- `render_fn`: rendering callback
- `alarm_fn`: alarm callback
- `push_enabled/push_interval_ms`: push policy

## 7.4 Optional Handler Interfaces
- `CustomHandleRun`
- `CustomHandleFrame`
- `CustomHandleControl`

Choose based on node role; you do not need to implement all of them.

---

## 8. Build, Deployment, and Runtime

## 8.1 Build Environment Requirements
- OS: Ubuntu 20.04/22.04 (recommended)
- Compiler: `aarch64-none-linux-gnu-gcc/g++`
- CMake: >= 3.10
- SDK download (runtime packages/toolchain): [Baidu Netdisk](https://pan.baidu.com/s/18CczjjNDnMhM15VDcAJcpQ), extraction code: `v8me`

## 8.2 One-Click Build

```bash
cd aibox_sdk
bash build_sdk_sample.sh
```

Script behavior:
1. exports toolchain PATH
2. builds root project (samples)
3. builds `plugins` and packages `.plugin` files

## 8.3 Manual Build (Recommended for CI)

```bash
cd aibox_sdk
export PATH="/home/work/ax/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-linux-gnu/bin:$PATH"

cmake -B build
cmake --build build -j

cmake -S plugins -B plugins/build
cmake --build plugins/build -j
```

Artifacts:
- sample binaries: `build/example/...`
- plugin packages: `plugins/build_out/*.plugin`

## 8.4 Deploy on Device

1. install runtime `.deb` on the board
2. copy plugin files to `/usr/local/aibox/plugins/`
3. set library path:

```bash
export LD_LIBRARY_PATH=/usr/local/aibox/lib:$LD_LIBRARY_PATH
```

4. restart service:

```bash
service aibox restart
```

5. watch logs:

```bash
journalctl -u aibox -f
```

## 8.5 Fast Acceptance Checklist
1. run `jdk_demo` to verify plugin loading and component schema output
2. create tasks in web UI and verify form rendering
3. trigger events and verify:
   - local alarm
   - HTTP push
   - snapshot persistence
   - TTS playback (if enabled)

---

## 9. Troubleshooting

### 9.1 `undefined symbol: plugin_init`
Cause: missing `extern "C"` export or wrong symbol name.

### 9.2 Model Loading Failure
Check:
- `model_files/model_path` in `config.json`
- model files are actually packaged
- runtime path and file permissions

### 9.3 Form Not Rendered or Rendered Incorrectly
Check:
- `component.parentType/type/label`
- each `component.formList` item has valid `type/key/name`
- proper multilingual object format

### 9.4 Push Failure
Check:
- `alarm_plugin.http_url`
- `alarm_push_uri/server_push_uri`
- JSON completeness
- request timeout and payload size

### 9.5 TTS Not Playing
Check:
- `tts_enabled=true`
- `tts_speaker_ip` reachable
- alarm payload contains `tts_text/tts_url/tts_queue`

### 9.6 Temporary SO Debugging
Set env:
- `AXPLUGIN_KEEP_TMP_SO=1` (preserve temporary extracted `.so`)

### 9.7 Encrypted Plugin Decryption Key
Set env:
- `PLUGIN_DECRYPT_KEY=<your_key>`

---

## 10. Additional Engineering Recommendations

## 10.1 Pre-Release Checklist
1. `config_template.json.in` keys match code-side key parsing
2. `plugin_init/plugin_cleanup` registration-unregistration pair is correct
3. `alarm_fn` contract follows unified schema
4. stress-test `push_interval_ms` and snapshot retention
5. complete one full end-to-end test against real backend

## 10.2 Runtime Resource Governance
`alarm_plugin` snapshot cleanup supports env vars:
- `ALARM_SNAPSHOT_RETENTION_DAYS` (default: 7)
- `ALARM_SNAPSHOT_MAX_BYTES_MB` (default: 500)

Recommend setting these consistently per project storage policy.

## 10.3 Performance Suggestions
- tune `det_threshold` and `push_interval_ms` to avoid push storms
- use `device_id/channel_id` for multi-stream resource isolation
- minimize upload payload by controlling picture types (`record_pic_type/server_pic_type`)

---

## Appendix A: Minimal Plugin Skeleton

```cpp
#include "sdk_interface.hpp"
#include "jdk_node_wrapper.hpp"

namespace jdk_nodes {
class MyNode : public jdk_node_base, public CustomHandleFrame, public CustomHandleControl {
public:
    MyNode(std::string name, const nlohmann::json& cfg) {}
    std::shared_ptr<jdk_objects::jdk_meta> handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) override {
        return jdk_node_base::handle_frame_meta(meta);
    }
    std::shared_ptr<jdk_objects::jdk_meta> handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) override {
        return meta;
    }
};
}

extern "C" void plugin_init(SDKInterface* sdk) {
    sdk->register_node("my_plugin", [](const std::string& name, const nlohmann::json& cfg) {
        return jdk_nodes::jdk_node_wrapper::create(name, std::make_shared<jdk_nodes::MyNode>(name, cfg));
    });
}

extern "C" void plugin_cleanup(SDKInterface* sdk) {
    sdk->unregister_node("my_plugin");
}
```

---

## Appendix B: References
- `doc/PLUGIN_CONFIG_REFERENCE_EN.md`
- `doc/平台设备对接协议规范-V1.0.2.docx`
- `plugins/alarm_plugin/TTS_README.md`

---

© AIBox SDK Team
