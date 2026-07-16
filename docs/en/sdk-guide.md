# AIBox SDK — Customer Development Guide

> - SDK `aibox_sdk`
> - Firmware `3.10.2`
> - Protocol Spec V1.0.2 (2026-02-21)
>
> **Target hardware**
>
> - ① AX650N / AX8850 standalone boards (all-in-one devices)
> - ② AX650N / AX8850 compute cards inserted into AX-based hosts (multi-card mix)
> - ③ Third-party hosts + AX compute cards (RK / Raspberry Pi 5 / x86)
> - ④ RK local devices (RK35xx / RV1126B, with native RK codec, RGA, and RKNN acceleration)
>
> **Audience**
>
> - Algorithm engineers
> - Backend engineers
> - Delivery / integration engineers

---

## Table of Contents
1. [SDK Overview](#1-sdk-overview)
2. [Repository Structure](#2-repository-structure)
3. [Built-in Algorithm Components](#3-built-in-algorithm-components)
4. [Plugin Development and Fast Algorithm Integration](#4-plugin-development-and-fast-algorithm-integration)
5. [Alarm Protocol and Message Specification](#5-alarm-protocol-and-message-specification)
6. [SDK Demo Guide](#6-sdk-demo-guide)
7. [Plugin Configuration Files](#7-plugin-configuration-files)
8. [Key SDK Interfaces](#8-key-sdk-interfaces)
9. [Build, Deployment, and Runtime](#9-build-deployment-and-runtime)
10. [Troubleshooting](#10-troubleshooting)
11. [Additional Engineering Recommendations](#11-additional-engineering-recommendations)

---

## 1. SDK Overview

### 1.1 Core Design

```text
┌─────────────────────────────────────────────────────────────┐
│                      AIBox Runtime                          │
│                                                             │
│  PluginLoader  ──scan──►  .plugin  ──dlopen──►  plugin_init │
│                                                      │      │
│  NodeFactory  ◄──register_node(type, creator)────────┘      │
│       │                                                     │
│       └──create()──► Node ──attach_to()──► Pipeline (DAG)   │
└─────────────────────────────────────────────────────────────┘
```

| Concept | Description |
|---|---|
| **Node** | Smallest processing unit — handles one stage: input, inference, or output |
| **Pipeline** | DAG of nodes connected via `attach_to()` |
| **Plugin** | Dynamically loaded `.plugin` package (`shared library + config.json + model files`) |
| **SDKInterface** | Host callbacks exposed to plugins (register nodes, log, read config, publish events) |

### 1.2 Typical Data Flow

```text
Input plugin          Algorithm plugin              Output plugin
netclient  ──frame──► persondet / firedet ──result──► alarm / record
wwaCam                facedet / lpr                  netserver / hdmi / gb28181
```

### 1.3 Alarm Pipeline

```text
Algorithm node
  └─ run_infer_combinations()
       └─ meta->result_map_[node_name].exchange(ResultEntry{
              result,           // inference output (std::any)
              render_fn,        // OSD overlay callback
              alarm_fn,         // alarm JSON builder callback
              push_interval_ms  // push cooldown (ms)
          })

alarm_plugin
  └─ polls result_map_ → calls alarm_fn()
       ├─ local_push_msg  →  local UI / DB / TTS
       ├─ server_push_msg →  POST server_push_uri (capture upload)
       └─ alarm_push_msg  →  POST /api/v1/device/report/event
```


### 1.4 Runtime Locations and RK Local Acceleration

AIBox Runtime supports a unified **Runtime Location** abstraction. Applications, plugins, and the Web UI select where a task runs; they do not need to know whether the underlying implementation is AX local hardware, an AXCL compute card, or RK local hardware.

| Runtime location | Typical hardware | Media pipeline | Inference backend | Description |
|---|---|---|---|---|
| `ax.local` | AX650N / AX8850 standalone devices | AX VDEC / IVPS / VENC | AX NPU | Full local AX SoC pipeline for all-in-one devices |
| `rk.local` | RK35xx / RV1126B RK platforms | RK MPP VDEC / RGA / MPP VENC | RKNN | Full local RK pipeline with reduced cross-device frame movement |
| `compute_card_N` | AXCL compute cards | AXCL VDEC / IVPS / VENC | AXCL NPU | Used by RK, Raspberry Pi, x86, or AX hosts with AXCL cards; `N` starts from 1 |

Design principles:

- **Local acceleration is exposed only when the platform supports it**: AX devices expose `ax.local`, RK devices expose `rk.local`; generic x86 / Raspberry Pi hosts do not expose a local NPU option.
- **Task-level closed-loop execution**: decode, image processing, inference, and encode are kept on the same runtime location by default to avoid moving large frames between RK and AXCL unnecessarily.
- **Stable plugin-facing API**: plugins receive common abstractions such as `AXVideoFrame` and `jdk_frame_meta`; the underlying frame may come from AX, AXCL, or RK.
- **RK local path is optimized for low-copy execution**: RK decode output, RGA processing, and RKNN input prefer dma-buf / MPP buffer handoff. Host synchronization is reserved for snapshots, debugging, or CPU-only algorithms.

Typical deployments:

```text
AX appliance:        RTSP -> AX VDEC   -> AX IVPS/RGA-equivalent -> AX NPU   -> AX VENC/WebRTC
RK local device:     RTSP -> RK MPP    -> RGA                    -> RKNN     -> RK MPP VENC/WebRTC
RK + AXCL card:      RTSP -> AXCL VDEC -> AXCL IVPS              -> AXCL NPU -> AXCL VENC/WebRTC
Generic host + AXCL: RTSP -> AXCL VDEC -> AXCL IVPS              -> AXCL NPU -> AXCL VENC/WebRTC
```

> Plugin developers should always create backends from the TaskManager-provided `runtime_location / runtime_device_id` fields instead of hard-coding platform branches inside plugins.
>
> - Detailed integration guide: [Runtime Location and Deployment](runtime-location.md)

---

## 2. Repository Structure

```text
aibox_sdk/
├── include/                    # Public SDK headers (required for plugin development)
│   ├── sdk_interface.hpp       # Plugin ↔ host bridge interface
│   ├── node_factory.hpp        # Node type registration and factory
│   ├── plugin_loader.hpp       # Plugin lifecycle (load/unload/hot-reload)
│   ├── alg_comm.hpp            # SafeAlgorithm wrapper (recommended)
│   ├── ax_algorithm_sdk.h      # AX algorithm C API and data structures
│   ├── RegionAnalyzer.hpp      # Region rules engine (intrusion/tripwire/loiter/crowd)
│   ├── draw_registry.hpp       # OSD draw strategy registry
│   └── draw_strategy.hpp       # OSD draw strategy abstract interface
├── plugins/                    # Plugin source code (reference implementations)
│   ├── netclient_plugin/       # Input: RTSP pull stream
│   ├── persondet_plugin/       # Algorithm: person detection (with region rules)
│   ├── absence_plugin/         # Algorithm: post abandonment / absence detection
│   ├── promptdet_plugin/       # Algorithm: open-vocabulary prompt detection
│   ├── facedet_plugin/         # Algorithm: face detection
│   ├── firedet_plugin/         # Algorithm: fire/smoke detection
│   ├── catdog_plugin/          # Algorithm: pet detection
│   ├── netserver_plugin/       # Output: RTSP push stream
│   └── hdmi_plugin/            # Output: HDMI display
├── example/                    # SDK sample programs
├── doc/                        # Protocol docs and config reference
│   ├── PLUGIN_CONFIG_REFERENCE_EN.md
│   └── USER_MANUAL/
├── build_sdk_sample.sh         # One-click build (samples + all plugins)
└── CMakeLists.txt
```

---

## 3. Built-in Algorithm Components

### 3.1 Input Components

| Node type | Label | Description |
| :-- | :-- | :-- |
| `netclient` | Network Pull Stream | RTSP pull input, supports time-based scheduling (`schedule_config`) |

### 3.2 Algorithm Components

> All plugins below are officially built-in. Select them directly when creating a task in the Web UI — no extra deployment needed.

#### 🧍 Person Security

| Plugin | Label | Key capability |
| :-- | :-- | :-- |
| `intrusion` | Intrusion Detection | Polygon zone enter/leave real-time alert |
| `loitering` | Loitering Detection | Target stays in zone beyond time threshold |
| `absence` | Post Abandonment / Absence Detection | Alerts when a staffed area becomes empty beyond the configured duration |
| `crowd` | Crowd Gathering | Object count in zone exceeds threshold |
| `tripwire` | Tripwire Detection | Virtual line cross + direction check (forward/reverse) |
| `peopleflow` | People Flow Statistics | Entry/exit counting + over-capacity alert |
| `humanattr` | Person Attribute | 25 attributes: age / gender / clothing / safety rope… |
| `facedet` | Face Detection | Face localization + quality score + OSD |
| `facerec` | Face Recognition | Feature extraction and comparison (allow/block list) |

#### 🚗 Vehicle Management

| Plugin | Label | Key capability |
| :-- | :-- | :-- |
| `lprV2` | License Plate Recognition | Plate number + vehicle type (V2, higher accuracy) |
| `illpark` | Illegal Parking Detection | Restricted zone + overtime parking alert |
| `vehicleflow` | Vehicle Flow Statistics | Entry/exit counting + over-capacity alert |
| `wrongway` | Wrong-way Detection | Detects vehicles traveling in prohibited direction |

#### 🔥 Safety Production

| Plugin | Label | Key capability |
| :-- | :-- | :-- |
| `firesmoke` | Fire & Smoke Detection | Flame / smoke, supports LLM second-pass review |
| `helmet` | Safety Helmet Detection | Alerts when helmet is not worn |

#### 🎭 Behavior Recognition

| Plugin | Label | Key capability |
| :-- | :-- | :-- |
| `fight` | Fighting Detection | Physical altercation recognition |
| `falldown` | Fall Detection | Person falling posture recognition |
| `calling` | Phone-calling Detection | Detects hand-held phone calling behavior |
| `smoking` | Smoking Detection | Smoking behavior recognition |

#### 🧠 Open Vocabulary and Intelligent Review

| Plugin | Label | Key capability |
| :-- | :-- | :-- |
| `promptdet` | Prompt Detection | Open-vocabulary object detection from configurable prompts, with regions, rules, tracking de-duplication, and alarm push |
| `llm` | LLM Second-pass Review | Calls external LLM to semantically validate alert frames, reducing false positives |

#### 🔧 Utility

| Plugin | Label | Key capability |
| :-- | :-- | :-- |
| `osd` | OSD Overlay | Caption / logo / timestamp watermark |

### 3.3 Output Components

| Plugin | Label | Description |
| :-- | :-- | :-- |
| `alarm` | Alarm Push | Unified routing: HTTP / TTS / local storage |
| `record` | Video Recording | Triggered snapshot / video capture to disk |
| `netserver` | Network Streaming | RTSP push encoding |
| `hdmi` | HDMI Output | Local HDMI display with OSD overlay |
| `gb28181` | GB28181 | National standard video networking protocol |
| `p2p` | P2P Live | Real-time P2P preview stream |

### 3.4 Region Rules Engine

`persondet` integrates `RegionAnalyzer`. **One plugin covers 6 security scenarios** depending on what shape is drawn in the UI and which toggles are enabled:

```text
UI action                          Rule triggered
──────────────────────────────────────────────────────────────
Draw polygon + enter toggle    →   Intrusion detection (Enter/Leave)
Draw polygon + loiter toggle   →   Loitering detection (loiter_sec timeout)
Draw polygon + absence toggle  →   Post abandonment (absence_sec no-person timeout)
Draw polygon + crowd toggle    →   Crowd gathering (crowd_threshold count)
Draw 2-point line              →   Tripwire direction detection (line_cross)
No region drawn                →   Full-frame person detection
```

Key config parameters:

| Key | Default | Description |
| :-- | :-- | :-- |
| `region_enable_enter` | `true` | Enable enter/leave alert |
| `region_enable_loiter` | `false` | Enable loitering detection |
| `region_loiter_sec` | `10` | Loiter trigger duration (s) |
| `region_loiter_cooldown` | `30` | Loiter alert cooldown (s) |
| `region_enable_absence` | `false` | Enable post abandonment |
| `region_absence_sec` | `30` | Absence trigger duration (s) |
| `region_enable_crowd` | `false` | Enable crowd gathering |
| `region_crowd_threshold` | `10` | Crowd trigger count |
| `region_enable_line_cross` | `false` | Enable tripwire |

### 3.5 Open-vocabulary Prompt Detection

`promptdet` is designed for scenarios where the target category changes often and retraining a model for each project is not practical. Users configure English prompts in the Web UI, such as `person`, `bus`, `traffic cone`, `trash bag`, or `cardboard box`. The system understands the prompt semantics and detects the corresponding targets in real-time video streams.

Core capabilities:

- **Dynamic prompts**: up to 5 English prompts per task; one prompt per line, and spaces inside a prompt are supported.
- **Unified resource delivery**: detection capability, prompt understanding capability, and required resources are delivered together; the plugin automatically selects the proper runtime backend for the current platform.
- **Region-aware detection**: no region means full-frame detection; polygon regions restrict detection and alarms to configured areas.
- **Generic event rules**: supports presence, absence, stationary object, new object, count threshold, and action-candidate rules.
- **Alarm governance**: includes lightweight tracking, continuous-frame confirmation, repeated-alarm interval, local Web alarms, and server-side alarm push for long-running video workloads.

Typical use cases:

| Scenario | Suggested prompts | Suggested rule |
| :-- | :-- | :-- |
| Doorstep package detection | `package` / `cardboard box` | New object or presence |
| Corridor clutter detection | `box` / `trash bag` / `carton` | Stationary |
| Road obstacle detection | `traffic cone` / `debris` | Presence or stationary |
| Missing safety equipment | `fire extinguisher` / `helmet` | Absence |
| Action candidate trigger | `cigarette` / `smoke` | Action candidate, preferably followed by second-stage review |

> English prompts are recommended for the current version. Non-English prompts may run, but semantic alignment is not guaranteed for production deployments.

---

## 4. Plugin Development and Fast Algorithm Integration

## 4.1 Plugin Lifecycle (Must Know)

Every plugin must export two C functions:

```cpp
extern "C" void plugin_init(SDKInterface* sdk);
extern "C" void plugin_cleanup(SDKInterface* sdk);
```

Runtime flow:

1. `PluginLoader` scans `.plugin` files.
2. `plugin_init` calls `sdk->register_node(type, creator)`.
3. On unload, `plugin_cleanup` calls `sdk->unregister_node(type)`.

## 4.2 Standard Plugin Development Procedure

### Step 1: Clone the Closest Existing Plugin Template
Recommended starting points: `persondet_plugin`, `firedet_plugin`, `netclient_plugin`.

> Directory naming recommendation: use `xxx_plugin` suffix. `plugins/CMakeLists.txt` auto-discovers subprojects using the `*_plugin` pattern.

### Step 2: Define Node Parameter Struct
Define `NodeParams` in `JdkXXXNode.hpp` and keep defaults centralized.

### Step 3: Parse Config in `plugin_infer.cpp`
Use `jp(config, key, default)` and register node:

```cpp
sdk->register_node(PLUGIN_NODE_NAME, [](const std::string& name, const nlohmann::json& config) {
    const auto runtime = PluginRuntime::from_task_config(config);

    auto nodeParams = std::make_unique<MyNodeParams>();
    nodeParams->threshold = jp(config, "threshold", 0.8f);
    nodeParams->runtime_location = runtime.location;          // ax.local / rk.local / compute_card_N
    nodeParams->runtime_device_id = runtime.runtime_device_id; // local=-1, compute_card_N=N-1
    nodeParams->model_path = runtime.is_rk_local()
        ? jp(config, "model_path_rk", "./models/xxx.rknn")
        : jp(config, "model_path_ax", "./models/xxx.axmodel");

    return jdk_nodes::jdk_node_wrapper::create(
        name,
        std::make_shared<jdk_nodes::MyNode>(name, std::move(nodeParams), runtime));
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
For AX algorithm SDK integration, prefer `SafeAlgorithm`. If the plugin also supports RKNN, select the backend during construction from `PluginRuntime`:

```cpp
SafeAlgorithm::Options opt{ax_model_type_fire_smoke, nodeParams_->model_path, nodeParams_->runtime_device_id};
alg_ = std::make_shared<SafeAlgorithm>(opt);
alg_->set_affinity(true);
alg_->update_params([&](auto &p) {
    p.det_threshold = 0.8f;
});
```

Dual RK / AX backend example:

```cpp
// runtime.infer_type(): rk.local -> "rk", ax.local / compute_card_N -> "ax"
infer_ = YOLOV5FACE::create_infer(model_path,
                                   runtime.infer_type(),
                                   runtime.runtime_device_id);
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

## 4.3 Fast Integration of Your Own Algorithm

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

## 4.4 Recommended Pipeline Templates

### Person Detection
`netclient_plugin -> persondet_plugin -> alarm_plugin`

### Face Recognition
`netclient_plugin -> facerec_plugin -> alarm_plugin (+ record_plugin optional)`

### Vehicle Capture
`netclient_plugin -> lpr_plugin -> alarm_plugin/gb28181_plugin`

## 4.5 Development Notes
- Node `type` must match `config.json.component.type`.
- `plugin_init/plugin_cleanup` must be exported correctly, otherwise: `undefined symbol: plugin_init`.
- Empty object from `alarm_fn` means no alarm processing in `alarm_plugin`.
- `push_interval_ms` is enforced at `ResultEntry` granularity.

---

## 5. Alarm Protocol and Message Specification

Two layers are involved:

- **Layer A (intra-SDK payload contract)**: between algorithm plugins and output plugins.
- **Layer B (device-to-platform protocol)**: HTTP reporting from device to platform.

## 5.1 Layer A: Unified Alarm JSON Structure

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

## 5.2 Layer A Field Constraints (Recommended)

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

## 5.3 `EventType` Mapping (Code Baseline)

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

## 5.4 Layer B: Device External HTTP Protocol

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

## 5.5 Protocol Version Compatibility (Important)

From code vs protocol doc comparison:

- protocol doc V1.0.2 examples mainly cover event values up to `14`
- SDK code currently extends `EventType` to `20` (e.g., `19/20`)

Recommendations:

1. backend enum parser should be forward-compatible (do not hard-reject unknown values)
2. classify using both `type` and `alarm_type`

## 5.6 Alarm Link Joint-Debug Checklist
1. verify algorithm plugin returns `alarm_count > 0`
2. verify `alarm_push_uri/server_push_uri` are non-empty when expected
3. verify `alarm_plugin.http_url` reachability
4. verify `did/type/time` fields are present
5. check image payload size (`data.bg_data`) for timeout risk

---

## 6. SDK Demo Guide

`example/` programs and their purpose:

| Demo | Purpose | Key Classes |
| :-- | :-- | :-- |
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

## 7. Plugin Configuration Files

## 7.1 Configuration Layers

### A. Plugin package metadata (`config.json` top-level)
- `name/version/platform/entry/md5/type`
- `model_path/model_files`
- `component`

### B. Frontend component schema (`component`)
- `parentType`: `input-component / algorithm-component / output-component / other-component`
- `type`: node type (must match registration name)
- `label`: display name
- `component.formList`: form schema

## 7.2 Common `formList` Controls
See [Plugin Configuration Reference](reference/plugin-config-full.md)

Core supported types:

- `input` / `inputNumber` / `password` / `textarea`
- `select` / `switch` / `slider`
- `button` / `divider` / `subdivider`
- `schedule` / `regionDraw`
- `readOnly` / `status`

## 7.3 Typical Configuration Patterns

### Input plugins
- `rtsp_url`
- `schedule_config`

### Algorithm plugins
- threshold, model path, push strategy, picture-type bitmask
- optional: `llm_review_*`, `tts_*`

### Output plugins
- `http_url`
- `tts_enabled/tts_speaker_ip/tts_volume/...`

## 7.4 Change Management Rules
1. new fields should always have defaults
2. avoid renaming old fields unless strictly necessary
3. any backend-parsed field changes require coordinated version update

---

## 8. Key SDK Interfaces

## 8.1 `SDKInterface` (Plugin Runtime Capabilities)

```cpp
struct SDKInterface {
    std::function<void(const std::string&, NodeCreator)> register_node;
    std::function<void(const std::string&)> unregister_node;
    std::function<void(const std::string& level, const std::string& message)> log;
    std::function<std::string(const std::string& key)> get_config;
    std::function<void(const std::string& topic, const std::string& payload)> publish_event;
};
```

## 8.2 `jdk_node_base` (Node Base Class)
Common APIs:

- `start()/stop()`
- `attach_to()/detach_recursively()`
- `make_frame_meta()/out_queue_push()`
- `is_alive()/set_alive()`

## 8.3 `jdk_frame_meta::result_map_`
`result_map_` is the result exchange bus:
`unordered_map<string, AtomicPtr<ResultEntry>>`

`ResultEntry` includes:

- `result`: inference output
- `render_fn`: rendering callback
- `alarm_fn`: alarm callback
- `push_enabled/push_interval_ms`: push policy

## 8.4 Optional Handler Interfaces
- `CustomHandleRun`
- `CustomHandleFrame`
- `CustomHandleControl`

Choose based on node role; you do not need to implement all of them.

## 8.5 `PluginRuntime` (Runtime Location Normalization)

TaskManager injects standardized runtime fields into each node config:

```json
{
  "runtime_location": "rk.local",
  "device_id": -1,
  "runtime_device_id": -1
}
```

Plugins should parse these fields through `PluginRuntime::from_task_config(config)` instead of using legacy `device_id` checks as platform detection.

```cpp
const auto runtime = PluginRuntime::from_task_config(config);

if (runtime.is_rk_local()) {
    // RK local: RK MPP / RGA / RKNN, runtime.runtime_device_id == -1
} else if (runtime.is_ax_local()) {
    // AX local: AX VDEC / IVPS / NPU / VENC, runtime.runtime_device_id == -1
} else if (runtime.is_compute_card()) {
    // AXCL compute card: runtime.runtime_device_id is zero-based
}

const char* backend = runtime.infer_type(); // rk.local -> "rk", AX paths -> "ax"
```

Resource isolation rules:

- `rk.local` and `ax.local` are local runtimes and use `runtime_device_id=-1`.
- `compute_card_1` maps to `runtime_device_id=0`, `compute_card_2` maps to `1`, and so on.
- Decoder, encoder, and image-processing channels are allocated per `runtime_location`.
- Generic hosts without a local media/NPU backend must not treat `local` as an inference backend.


---

## 9. Build, Deployment, and Runtime

## 9.1 Build Environment Requirements
- OS: Ubuntu 20.04/22.04 (recommended)
- Compiler: `aarch64-none-linux-gnu-gcc/g++`
- CMake: >= 3.10
- SDK download (runtime packages/toolchain):
  - [Baidu Netdisk](https://pan.baidu.com/s/18CczjjNDnMhM15VDcAJcpQ?pwd=v8me), extraction code: `v8me`
  - [Google Drive](https://drive.google.com/drive/folders/15cmvIBABTxfgwNvJvhgTI9tMyAiH8vVT?usp=drive_link)

## 9.2 One-Click Build

```bash
cd aibox_sdk
bash build_sdk_sample.sh
```

Script behavior:

1. exports toolchain PATH
2. builds root project (samples)
3. builds `plugins` and packages `.plugin` files

## 9.3 Manual Build (Recommended for CI)

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

## 9.4 Deploy on Device

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

## 9.5 Fast Acceptance Checklist
1. run `jdk_demo` to verify plugin loading and component schema output
2. create tasks in web UI and verify form rendering
3. trigger events and verify:
   - local alarm
   - HTTP push
   - snapshot persistence
   - TTS playback (if enabled)

## 9.6 RK Local Runtime Environment

When the selected runtime location is `rk.local`, the target board must provide Rockchip runtime libraries and drivers:

| Capability | Dependency | Purpose |
|---|---|---|
| Video decode / encode | RK MPP | RTSP H.264/H.265 decode, encoded output, WebRTC preview |
| Image processing | librga / RGA driver | OSD composition, resize, crop, format conversion, affine transform |
| NPU inference | RKNN Runtime | `.rknn` model loading and execution |
| Frame memory | dma-buf / MPP buffer | Reduces large-frame copies between MPP, RGA, and RKNN |

Recommended checks:

```bash
ls /dev/rga /dev/mpp_service 2>/dev/null
ldd /usr/local/aibox/bin/TaskManager | grep -E 'rga|mpp|rknn'
export LD_LIBRARY_PATH=/usr/local/aibox/lib:$LD_LIBRARY_PATH
```

RK local development notes:

- Provide RKNN models separately, preferably via `model_path_rk`.
- Keep real-time MPP / RGA frames inside the common `AXVideoFrame` abstraction; avoid calling `toHost()` on every frame unless a CPU algorithm, snapshot, or debug dump truly needs it.
- RGA / RKNN scheduling is managed by the runtime; plugins should not create global hardware-context singletons for multi-task workloads.


---

## 10. Troubleshooting

### 10.1 `undefined symbol: plugin_init`
Cause: missing `extern "C"` export or wrong symbol name.

### 10.2 Model Loading Failure
Check:

- `model_files/model_path` in `config.json`
- model files are actually packaged
- runtime path and file permissions

### 10.3 Form Not Rendered or Rendered Incorrectly
Check:

- `component.parentType/type/label`
- each `component.formList` item has valid `type/key/name`
- proper multilingual object format

### 10.4 Push Failure
Check:

- `alarm_plugin.http_url`
- `alarm_push_uri/server_push_uri`
- JSON completeness
- request timeout and payload size

### 10.5 TTS Not Playing
Check:

- `tts_enabled=true`
- `tts_speaker_ip` reachable
- alarm payload contains `tts_text/tts_url/tts_queue`

### 10.6 Temporary SO Debugging
Set env:

- `AXPLUGIN_KEEP_TMP_SO=1` (preserve temporary extracted `.so`)

### 10.7 Encrypted Plugin Decryption Key
Set env:

- `PLUGIN_DECRYPT_KEY=<your_key>`

### 10.8 RK Local Runtime Does Not Appear in the Web UI
Check:

- whether the target is an RK / RV / Rockchip platform (`cat /proc/device-tree/compatible`)
- whether RK runtime libraries are installed with the firmware
- generic x86 / Raspberry Pi hosts expose only `compute_card_N` when AXCL cards are present; they do not expose `rk.local` or `ax.local`

### 10.9 RK Local Video Works but Inference Looks Wrong
Check:

- the plugin uses `PluginRuntime` and loads `.rknn` under `rk.local`
- preprocessing matches the model input layout, commonly NV12 -> resize/letterbox -> RGB/NHWC
- the real-time path does not call `toHost()` unnecessarily on every MPP/RGA frame
- temporarily enable `AIBOX_RK_IVPS_DIAG=1` for RGA path diagnostics, then turn it off in production

Useful RK diagnostics:

- `AIBOX_RK_IVPS_DIAG=1`: print RK RGA / IVPS diagnostics
- `AIBOX_RKNN_DMA_INPUT=1`: try RKNN dma-buf input binding, only when the RKNN Runtime and model input format support it


---

## 11. Additional Engineering Recommendations

## 11.1 Pre-Release Checklist
1. `config_template.json.in` keys match code-side key parsing
2. `plugin_init/plugin_cleanup` registration-unregistration pair is correct
3. `alarm_fn` contract follows unified schema
4. stress-test `push_interval_ms` and snapshot retention
5. complete one full end-to-end test against real backend

## 11.2 Runtime Resource Governance
`alarm_plugin` snapshot cleanup supports env vars:

- `ALARM_SNAPSHOT_RETENTION_DAYS` (default: 7)
- `ALARM_SNAPSHOT_MAX_BYTES_MB` (default: 500)

Recommend setting these consistently per project storage policy.

## 11.3 Performance Suggestions
- tune `det_threshold` and `push_interval_ms` to avoid push storms
- use `runtime_location` for resource isolation: `ax.local` for AX local, `rk.local` for RK local, and `compute_card_N` for AXCL cards
- minimize upload payload by controlling picture types (`record_pic_type/server_pic_type`)
- keep RK local pipelines closed-loop through MPP/RGA/RKNN and avoid per-frame CPU copies for large NV12/RGB buffers

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
- [User Manual](user-manual/index.md)
- [Plugin Configuration Reference](reference/plugin-config-full.md)
- [Platform Device Integration Protocol V1.0.2](../assets/downloads/platform-device-integration-protocol-v1.0.2.docx)
- [Text TTS and Media URL Playback API](../assets/downloads/text-tts-media-url-api.pdf)

---

© AIBox SDK Team
