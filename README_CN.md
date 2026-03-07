<div align="left">
<a href="README_EN.md"><button>English</button></a>
</div>

# AIBox SDK 客户开发指南

**适用范围**
- SDK 版本：`aibox_sdk`（当前仓库）
- 固件基线：`3.6.2`
- 对接协议版本：`平台设备对接协议规范 V1.0.2`（发布时间：`2026-02-21`）
- 目标硬件：AX650N / AX8850（板卡与算力卡）
- 目标读者：算法工程师、平台后端工程师、交付实施工程师

---

## 目录
1. [SDK 总体说明](#1-sdk-总体说明)
2. [目录与工程结构](#2-目录与工程结构)
3. [插件开发与算法快速集成（重点）](#3-插件开发与算法快速集成重点)
4. [报警协议与报文规范（重点）](#4-报警协议与报文规范重点)
5. [SDK Demo 说明](#5-sdk-demo-说明)
6. [插件配置文件使用说明](#6-插件配置文件使用说明)
7. [SDK 关键接口说明](#7-sdk-关键接口说明)
8. [编译、部署与运行](#8-编译部署与运行)
9. [常见问题与排障](#9-常见问题与排障)
10. [其它工程化建议](#10-其它工程化建议)

---

## 1. SDK 总体说明

### 1.1 核心设计
- **Node（节点）**：最小处理单元，负责输入、算法、输出中的某个环节。
- **Pipeline（管线）**：通过 `attach_to()` 将多个节点按 DAG 连接。
- **Plugin（插件）**：运行时动态加载的功能包（`.plugin`），包含 `libxxx.so + config.json + models`。
- **SDKInterface**：运行时向插件提供的回调入口（注册节点、日志、配置、事件）。

### 1.2 典型数据流
`输入插件(netclient/wwaCam)` -> `算法插件(persondet/firedet/...)` -> `输出插件(alarm/record/gb28181/p2p/netserver/hdmi)`

### 1.3 报警链路（高频对接场景）
- 算法节点通过 `result_map_` 写入 `ResultEntry`。
- `ResultEntry` 包含：`result + render_fn + alarm_fn + push策略`。
- `alarm_plugin` 拉取各算法节点 `alarm_fn` 的返回结果并统一推送：
  - 本地通知（UI/数据库）
  - HTTP 服务器推送
  - 事件上报 `/api/v1/device/report/event`
  - TTS 音柱播报

---

## 2. 目录与工程结构

```text
aibox_sdk/
├── include/                       # SDK 对外头文件（插件开发必须）
│   ├── sdk_interface.hpp
│   ├── node_factory.hpp
│   ├── plugin_loader.hpp
│   ├── alg_comm.hpp
│   └── ax_algorithm_sdk.h
├── plugins/                       # 插件实现（算法/输入/输出）
│   ├── netclient_plugin/
│   ├── persondet_plugin/
│   ├── firedet_plugin/
│   ├── alarm_plugin/
│   └── ...
├── example/                       # SDK 使用样例
├── doc/
│   ├── PLUGIN_CONFIG_REFERENCE_ZH.md
│   └── 平台设备对接协议规范-V1.0.2.docx
├── build_sdk_sample.sh            # 一键构建示例与插件
└── CMakeLists.txt
```

---

## 3. 插件开发与算法快速集成（重点）

## 3.1 插件生命周期（必须理解）

每个插件必须导出两个 C 接口：

```cpp
extern "C" void plugin_init(SDKInterface* sdk);
extern "C" void plugin_cleanup(SDKInterface* sdk);
```

运行时流程：
1. `PluginLoader` 扫描 `.plugin` 文件。
2. `plugin_init` 内部调用 `sdk->register_node(type, creator)`。
3. 卸载时调用 `plugin_cleanup`，执行 `sdk->unregister_node(type)`。

## 3.2 插件开发标准步骤

### 步骤 1：复制一个最接近的模板插件
推荐从 `persondet_plugin` / `firedet_plugin` / `netclient_plugin` 复制。

> 目录命名建议使用 `xxx_plugin`（以 `_plugin` 结尾）。当前 `plugins/CMakeLists.txt` 默认通过 `*_plugin` 模式自动发现子工程。

### 步骤 2：定义节点参数结构
在 `JdkXXXNode.hpp` 中定义 `NodeParams`，集中管理配置项默认值。

### 步骤 3：在 `plugin_infer.cpp` 解析配置
使用 `jp(config, key, default)` 读取参数，构造节点对象并注册：

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

### 步骤 4：实现节点类（算法核心）
建议继承：
- `jdk_node_base`
- `CustomHandleFrame`
- `CustomHandleControl`

最少实现：
- `handle_frame_meta(std::shared_ptr<jdk_frame_meta>)`
- `handle_control_meta(...)`
- （可选）`run_infer_combinations(...)`

### 步骤 5：接入算法推理
若使用 AX 算法 SDK，优先使用 `SafeAlgorithm` 封装：

```cpp
SafeAlgorithm::Options opt{ax_model_type_fire_smoke, nodeParams_->model_path, nodeParams_->device_id};
alg_ = std::make_shared<SafeAlgorithm>(opt);
alg_->set_affinity(true);
alg_->update_params([&](auto &p) {
    p.det_threshold = 0.8f;
});
```

常用推理接口：
- `detect(frame, result, capture)`
- `track(frame, result, capture)`
- `get_body_attr(...)`
- `get_plate(...)`
- `get_face_feature_2(...)`

### 步骤 6：将算法结果写入 `result_map_`
核心是构造 `jdk_objects::ResultEntry`：
- `result`：算法结果对象（通常 `std::any`）
- `render_fn`：OSD 绘制回调
- `alarm_fn`：报警报文构造回调
- `push_enabled/push_interval_ms`：上报策略

### 步骤 7：定义 `alarm_fn` 返回报文
**强烈建议遵循统一结构**（第 4 章详述），否则 `alarm_plugin` 无法正确路由到 HTTP/TTS/存储。

### 步骤 8：配置模板 `config_template.json.in`
- 定义组件归属：`input-component / algorithm-component / output-component / other-component`
- 定义前端可视化配置：`component.formList`

### 步骤 9：编译并打包 `.plugin`
`plugins/CMakeLists.txt` 会自动遍历 `*_plugin` 子目录，构建并打包。

### 步骤 10：部署验证
- 将 `.plugin` 拷贝到设备插件目录。
- 重启服务，确认节点注册成功。
- 用前端创建任务验证完整链路。

## 3.3 快速集成自有算法（建议路径）

### 路径 A：替换现有算法插件推理核心（推荐）
适合：结构相似（输入输出一致）的检测算法替换。

执行方式：
1. 复制现有插件（如 `persondet_plugin`）。
2. 保留 `render_fn/alarm_fn/config_template`。
3. 仅替换 `run_infer_combinations()` 内部推理逻辑。
4. 保持 `alarm_fn` 字段结构不变。

优点：
- 最快上线。
- 与现有前端/后端协议兼容性最好。

### 路径 B：新建插件类型
适合：全新任务类型或完全不同业务逻辑。

必须同步：
- 新 `type`（注册名）
- 新 `config_template`
- 新 `alarm_type` 与 `EventType` 映射
- 后端新类型识别逻辑

## 3.4 推荐管线模板

### 人员检测场景
`netclient_plugin -> persondet_plugin -> alarm_plugin`

### 人脸识别场景
`netclient_plugin -> facerec_plugin -> alarm_plugin (+ record_plugin 可选)`

### 车辆抓拍场景
`netclient_plugin -> lpr_plugin -> alarm_plugin/gb28181_plugin`

## 3.5 开发注意事项
- 节点 `type` 必须与 `config.json.component.type` 一致。
- `plugin_init/plugin_cleanup` 必须可导出，否则会出现 `undefined symbol: plugin_init`。
- `alarm_fn` 返回空对象时，`alarm_plugin` 会跳过该次推送。
- `push_interval_ms` 在 `ResultEntry` 粒度生效。

---

## 4. 报警协议与报文规范（重点）

本章分两层：
- **层 A（插件内部协议）**：算法插件与输出插件之间的 JSON 协议。
- **层 B（设备外部协议）**：设备向平台 HTTP 上报协议。

## 4.1 层 A：插件内部统一报警结构

算法插件 `alarm_fn` 推荐返回：

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
        "tts_text": "注意，检测到异常行为"
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

`alarm_plugin` 处理规则：
1. 读取根级 `server_push_uri/alarm_push_uri`。
2. 遍历 `alarms[]`。
3. 若有 `server_push_msg` 则 POST 到 `server_push_uri`。
4. 若有 `alarm_push_msg` 则 POST 到 `alarm_push_uri`。
5. `local_push_msg` 用于本地通知与 TTS 提取，不对外 HTTP 推送。

## 4.2 层 A 字段约束（建议）

### 根对象字段
- `alarm_type`：字符串，报警类型标识（供本地与后端分类）。
- `timestamp`：ISO8601 字符串。
- `alarm_count`：整数。
- `alarms`：数组。
- `alarm_push_uri/server_push_uri`：可为空，空则不推送该通道。

### 单条报警字段
- `local_push_msg`：本地展示、数据库入库、TTS 字段载体。
- `server_push_msg`：结构化业务上报（如人脸/车辆抓拍）。
- `alarm_push_msg`：事件上报标准载体（`did/type/time/state/data`）。

### TTS 字段（本地）
可放在 `local_push_msg`：
- `tts_text`
- `tts_url`
- `tts_queue`（数组，队列播报）

## 4.3 `EventType` 映射（代码基准）

来源：`thirdpark/comm/include/DevProtoDef.hpp`

| 值 | 含义 |
|---|---|
| 1 | MOTION_DETECTION（移动侦测） |
| 2 | INTRUSION_DETECTION（入侵/绊线） |
| 3 | VIDEO_BLIND（视频遮挡） |
| 4 | BABY_CRYING |
| 5 | CROWD_GATHERING |
| 6 | PERSON_LOITERING |
| 7 | FAST_MOVING |
| 8 | FIRE_ALARM（火焰） |
| 9 | SOUND_SPIKE |
| 10 | NOISE_DROP |
| 11 | HUMAN_DETECTION（人形抓拍） |
| 12 | WHITELIST_DETECTION |
| 13 | BLACKLIST_DETECTION |
| 14 | VIP_DETECTION |
| 15 | POST_ABANDONMENT（离岗） |
| 16 | FALL_DETECTION |
| 17 | VITAL_SIGN_DETECTION |
| 18 | KITCHEN_MONITORING |
| 19 | BEHAVIOR_DETECTION（行为检测） |
| 20 | NON_MOTORIZED_VEHICLE（非机动车） |

## 4.4 层 B：设备对外 HTTP 协议（平台规范）

根据 `平台设备对接协议规范 V1.0.2`：
- 事件上报：`POST /api/v1/device/report/event`
- 设备信息上报：`POST /api/v1/device/report/info`
- 人脸抓拍上报：`POST /api/v1/adapter/lenfocus/face/capture`
- 车辆抓拍上报：`POST /api/v1/adapter/lenfocus/vehicle/capture`

> 说明：部分算法插件源码里默认的 `server_push_uri`（如 `/api/v1/face/capture`）与协议文档路径可能存在网关层差异，落地时请以平台网关实际路由为准统一映射。

### `/api/v1/device/report/event` 标准体

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

## 4.5 协议版本兼容说明（重要）

基于代码与协议文档对比可见：
- 协议文档 V1.0.2 对事件类型示例主要覆盖到 `14`。
- SDK 当前 `EventType` 已扩展到 `20`（如 `19/20`）。

建议：
1. 后端事件类型枚举做前向兼容（未知值不要直接拒绝）。
2. 新增类型按 `type + alarm_type` 双字段联合识别。

## 4.6 报警链路联调清单
1. 算法插件是否产出 `alarm_count > 0`。
2. `alarm_push_uri/server_push_uri` 是否非空。
3. `http_url` 是否可达（`alarm_plugin` 配置）。
4. `did/type/time` 是否存在。
5. `data.bg_data` 图片大小是否超限（大包会导致接口超时）。

---

## 5. SDK Demo 说明

`example/` 目录样例用途如下：

| Demo | 作用 | 关键类 |
|---|---|---|
| `jdk_frame_sample` | 基础帧对象加载/保存 | `AXVideoFrame` |
| `jdk_capture_sample` | NV12/JPEG 抓拍 | `HwCapture` |
| `jdk_ivps_sample` | IVPS 处理（畸变矫正/色彩转换） | `HwIvps` |
| `jdk_npu_sample` | NPU 模型推理与结果绘制 | `YOLOFACE` + `HwIvps` |
| `jdk_alg_sample` | `SafeAlgorithm` 统一接口调用（如车牌识别） | `SafeAlgorithm` |
| `jdk_netclient_vdec_sample` | RTSP 拉流/取帧 | `NetClient` |
| `jdk_netserver_venc_sample` | 编码推流样例 | `HwEncoder` |
| `jdk_node_sample` | 动态加载插件并输出组件结构 | `PluginLoader` + `NodeFactory` |

> `jdk_venc_vdec_vo_sample/readme.txt` 说明：编码/解码/显示样例主要参考插件内实现，不单独给完整 demo。

### 建议验证顺序
1. `jdk_frame_sample` -> 验证文件读写。
2. `jdk_capture_sample` / `jdk_ivps_sample` -> 验证图像处理链。
3. `jdk_npu_sample` / `jdk_alg_sample` -> 验证算法推理。
4. `jdk_node_sample` + 插件 -> 验证插件加载与配置展示。

---

## 6. 插件配置文件使用说明

## 6.1 配置文件分层

### A. 插件包元数据（`config.json` 顶层）
- `name/version/platform/entry/md5/type`
- `model_path/model_files`
- `component`

### B. 前端组件定义（`component`）
- `parentType`：`input-component / algorithm-component / output-component / other-component`
- `type`：节点类型（必须与注册名一致）
- `label`：前端显示名
- `component.formList`：参数面板定义

## 6.2 `formList` 常用控件
详见：`doc/PLUGIN_CONFIG_REFERENCE_ZH.md`

已支持核心类型：
- `input` / `inputNumber` / `password` / `textarea`
- `select` / `switch` / `slider`
- `button` / `divider` / `subdivider`
- `schedule` / `regionDraw`
- `readOnly` / `status`

## 6.3 典型配置范式

### 输入插件（拉流）
- `rtsp_url`
- `schedule_config`（按时布控）

### 算法插件
- 阈值、模型路径、推送策略、图片类型位掩码
- 可选：`llm_review_*`、`tts_*`

### 输出插件
- `http_url`
- `tts_enabled/tts_speaker_ip/tts_volume/...`

## 6.4 配置变更原则
1. 新增字段应有默认值。
2. 老字段尽量兼容，不随意重命名。
3. 涉及后端解析的字段必须做版本同步。

---

## 7. SDK 关键接口说明

## 7.1 `SDKInterface`（插件入口能力）

```cpp
struct SDKInterface {
    std::function<void(const std::string&, NodeCreator)> register_node;
    std::function<void(const std::string&)> unregister_node;
    std::function<void(const std::string& level, const std::string& message)> log;
    std::function<std::string(const std::string& key)> get_config;
    std::function<void(const std::string& topic, const std::string& payload)> publish_event;
};
```

## 7.2 `jdk_node_base`（节点基类）
常用能力：
- `start()/stop()`
- `attach_to()/detach_recursively()`
- `make_frame_meta()/out_queue_push()`
- `is_alive()/set_alive()`

## 7.3 `jdk_frame_meta::result_map_`
`result_map_` 是算法结果交换总线，类型为：
`unordered_map<string, AtomicPtr<ResultEntry>>`

`ResultEntry` 包含：
- `result`：任意类型结果
- `render_fn`：渲染回调
- `alarm_fn`：报警回调
- `push_enabled/push_interval_ms`：上报策略

## 7.4 可选处理接口
- `CustomHandleRun`
- `CustomHandleFrame`
- `CustomHandleControl`

根据节点职责选择组合继承，不要求全部实现。

---

## 8. 编译、部署与运行

## 8.1 编译环境要求
- OS：Ubuntu 20.04/22.04（推荐）
- 编译器：`aarch64-none-linux-gnu-gcc/g++`
- CMake：>= 3.10
- SDK 下载地址（安装包/工具链）：[百度网盘](https://pan.baidu.com/s/18CczjjNDnMhM15VDcAJcpQ)，提取码：`v8me`

## 8.2 一键构建

```bash
cd aibox_sdk
bash build_sdk_sample.sh
```

脚本执行内容：
1. 设置交叉工具链 PATH。
2. 编译根工程（sample）。
3. 编译 `plugins` 子工程并打包 `.plugin`。

## 8.3 手动构建（推荐 CI 使用）

```bash
cd aibox_sdk
export PATH="/home/work/ax/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-linux-gnu/bin:$PATH"

cmake -B build
cmake --build build -j

cmake -S plugins -B plugins/build
cmake --build plugins/build -j
```

产物：
- 示例程序：`build/example/...`
- 插件包：`plugins/build_out/*.plugin`

## 8.4 板端部署

1. 安装运行时 `.deb`（板端）。
2. 拷贝插件到 `/usr/local/aibox/plugins/`。
3. 设置库路径：

```bash
export LD_LIBRARY_PATH=/usr/local/aibox/lib:$LD_LIBRARY_PATH
```

4. 重启服务：

```bash
service aibox restart
```

5. 查看日志：

```bash
journalctl -u aibox -f
```

## 8.5 快速验收
1. 运行 `jdk_demo` 验证插件加载与组件结构输出。
2. 在 Web 端创建任务，检查配置表单是否正确渲染。
3. 触发目标事件，确认：
   - 本地告警
   - HTTP 上报
   - 图像落盘
   - TTS 播报（若启用）

---

## 9. 常见问题与排障

### 9.1 `undefined symbol: plugin_init`
原因：未导出 `extern "C"` 或函数名错误。

### 9.2 模型加载失败
检查：
- `config.json` 中 `model_files/model_path`
- 包内是否含模型
- 运行时目录权限

### 9.3 表单不显示或显示异常
检查：
- `component.parentType/type/label`
- `component.formList` 的 `type/key/name`
- 多语言对象格式

### 9.4 上报失败
检查：
- `alarm_plugin.http_url`
- `alarm_push_uri/server_push_uri`
- 报文 JSON 字段完整性
- 接口超时与包体大小

### 9.5 TTS 不播报
检查：
- `tts_enabled=true`
- `tts_speaker_ip` 可达
- 算法报警里是否包含 `tts_text/tts_url/tts_queue`

### 9.6 插件临时 so 排查
设置：
- `AXPLUGIN_KEEP_TMP_SO=1`（保留临时 so 便于定位）

### 9.7 加密插件密钥
设置：
- `PLUGIN_DECRYPT_KEY=<your_key>`

---

## 10. 其它工程化建议

## 10.1 发布前检查清单
1. `config_template.json.in` 字段与代码读取键一致。
2. `plugin_init/plugin_cleanup` 成对注册注销。
3. `alarm_fn` 返回结构符合统一规范。
4. 压测 `push_interval_ms` 与快照空间策略。
5. 用真实后端完成一次完整联调。

## 10.2 运行期资源治理
`alarm_plugin` 支持快照清理参数：
- `ALARM_SNAPSHOT_RETENTION_DAYS`（默认 7）
- `ALARM_SNAPSHOT_MAX_BYTES_MB`（默认 500）

建议按项目磁盘策略统一配置。

## 10.3 性能建议
- 合理设置 `det_threshold`、`push_interval_ms`，避免过载告警。
- 多路场景使用 `device_id/channel_id` 做资源隔离。
- 优先控制上报图片种类（`record_pic_type/server_pic_type`）减少网络负载。

---

## 附录 A：最小插件骨架

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

## 附录 B：参考文档
- `doc/PLUGIN_CONFIG_REFERENCE_ZH.md`
- `doc/平台设备对接协议规范-V1.0.2.docx`
- `plugins/alarm_plugin/TTS_README.md`

---

© AIBox SDK Team
