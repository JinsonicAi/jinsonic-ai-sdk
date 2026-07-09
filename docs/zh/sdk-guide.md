# AIBox SDK — 客户开发指南

> - SDK `aibox_sdk`
> - 固件基线 `3.10.2`
> - 对接协议规范 V1.0.2（2026-02-21）
>
> **目标硬件**
>
> - ① AX650N / AX8850 独立板卡（独立设备）
> - ② AX650N / AX8850 算力卡插入 AX 主机（多卡混用）
> - ③ 第三方主机 + AX 算力卡（RK / 树莓派 5 / x86 等）
> - ④ RK 本地设备（RK35xx / RV1126B 等，支持直接调用 RK 本地编解码、RGA 与 RKNN）
>
> **目标读者**
>
> - 算法工程师
> - 平台后端工程师
> - 交付实施工程师

---

## 目录
1. [SDK 总体说明](#1-sdk-总体说明)
2. [目录与工程结构](#2-目录与工程结构)
3. [内置算法组件一览](#3-内置算法组件一览)
4. [插件开发与算法快速集成](#4-插件开发与算法快速集成)
5. [报警协议与报文规范](#5-报警协议与报文规范)
6. [SDK Demo 说明](#6-sdk-demo-说明)
7. [插件配置文件使用说明](#7-插件配置文件使用说明)
8. [SDK 关键接口说明](#8-sdk-关键接口说明)
9. [编译、部署与运行](#9-编译部署与运行)
10. [常见问题与排障](#10-常见问题与排障)
11. [其它工程化建议](#11-其它工程化建议)

---

## 1. SDK 总体说明

### 1.1 核心设计

```text
┌─────────────────────────────────────────────────────────────┐
│                      AIBox Runtime                          │
│                                                             │
│  PluginLoader  ──scan──►  .plugin包  ──dlopen──►  plugin_init│
│                                                      │      │
│  NodeFactory  ◄──register_node(type, creator)────────┘      │
│       │                                                     │
│       └──create()──► Node ──attach_to()──► Pipeline (DAG)   │
└─────────────────────────────────────────────────────────────┘
```

| 概念 | 说明 |
|---|---|
| **Node（节点）** | 最小处理单元，负责输入 / 算法推理 / 结果输出中的某个环节 |
| **Pipeline（管线）** | 通过 `attach_to()` 将多个节点按 DAG 连接形成处理链 |
| **Plugin（插件）** | 运行时动态加载的功能包（`.plugin`），内含 `共享库 + config.json + 模型文件` |
| **SDKInterface** | 宿主向插件暴露的回调接口（注册节点、日志、读取配置、发布事件） |

### 1.2 典型数据流

```text
输入插件              算法插件                   输出插件
netclient  ──帧──►  persondet / firedet  ──结果──►  alarm / record
wwaCam                facedet / lpr              netserver / hdmi / gb28181
```

### 1.3 报警链路（高频对接场景）

```text
算法节点
  └─ run_infer_combinations()
       └─ meta->result_map_[node_name].exchange(ResultEntry{
              result,          // 推理结果（std::any）
              render_fn,       // OSD 绘制回调
              alarm_fn,        // 报警报文构建回调
              push_interval_ms // 推送冷却（毫秒）
          })

alarm_plugin
  └─ 轮询 result_map_ 中各节点 alarm_fn()
       ├─ local_push_msg  →  本地 UI / 数据库 / TTS
       ├─ server_push_msg →  POST server_push_uri（业务抓拍上报）
       └─ alarm_push_msg  →  POST /api/v1/device/report/event
```


### 1.4 多运行位置与 RK 本地算力支持

AIBox Runtime 已支持统一的 **Runtime Location（运行位置）** 抽象。上层任务、插件和前端只需要选择任务运行位置，不需要关心底层是 AX 本地、AXCL 计算卡还是 RK 本地硬件。

| 运行位置 | 典型硬件 | 媒体链路 | 推理后端 | 说明 |
|---|---|---|---|---|
| `ax.local` | AX650N / AX8850 独立设备 | AX VDEC / IVPS / VENC | AX NPU | AX SoC 本地闭环处理，适合一体机部署 |
| `rk.local` | RK35xx / RV1126B 等 RK 平台 | RK MPP VDEC / RGA / MPP VENC | RKNN | RK 本地闭环处理，减少跨设备搬运，适合 RK 主机独立部署 |
| `compute_card_N` | AXCL 计算卡 | AXCL VDEC / IVPS / VENC | AXCL NPU | RK / 树莓派 / x86 / AX 主机插卡时使用，`N` 从 1 开始 |

核心原则：

- **本地算力只在平台明确支持时开放**：AX 平台显示 `ax.local`，RK 平台显示 `rk.local`；普通 x86 / 树莓派等 Host 不会暴露本地 NPU 选项。
- **任务级闭环运行**：同一任务的解码、图像处理、推理、编码默认在同一个运行位置完成，避免 RK 与 AXCL 之间频繁搬运大帧数据。
- **插件接口保持一致**：插件统一接收 `AXVideoFrame` / `jdk_frame_meta` 等公共抽象；底层帧可能来自 AX、AXCL 或 RK，但上层算法节点不需要直接操作平台私有结构。
- **RK 本地路径面向零拷贝优化**：RK 解码输出、RGA 处理、RKNN 输入优先使用 dma-buf / MPP buffer 传递；只有抓拍、调试保存或 CPU 算法确实需要时才同步到 Host。

典型部署形态：

```text
AX 一体机：        RTSP -> AX VDEC  -> AX IVPS/RGA-equivalent -> AX NPU  -> AX VENC/WebRTC
RK 本地设备：      RTSP -> RK MPP   -> RGA                   -> RKNN    -> RK MPP VENC/WebRTC
RK + AXCL 计算卡： RTSP -> AXCL VDEC -> AXCL IVPS             -> AXCL NPU -> AXCL VENC/WebRTC
普通 Host + AXCL：RTSP -> AXCL VDEC -> AXCL IVPS             -> AXCL NPU -> AXCL VENC/WebRTC
```

> 对插件开发者而言，推荐始终从 TaskManager 注入的 `runtime_location / runtime_device_id` 创建后端，不要在插件内部写死平台分支。
>
> - 详细适配指南：[运行位置与部署](runtime-location.md)

---

## 2. 目录与工程结构

```text
aibox_sdk/
├── include/                    # SDK 对外头文件（插件开发必读）
│   ├── sdk_interface.hpp       # 插件与宿主的桥接接口
│   ├── node_factory.hpp        # 节点类型注册与创建工厂
│   ├── plugin_loader.hpp       # 插件生命周期管理（加载/卸载/热重载）
│   ├── alg_comm.hpp            # SafeAlgorithm 封装（推荐使用）
│   ├── ax_algorithm_sdk.h      # AX 算法底层 C 接口与数据结构
│   ├── RegionAnalyzer.hpp      # 区域规则引擎（入侵/绊线/离岗/拥挤）
│   ├── draw_registry.hpp       # OSD 绘制策略注册
│   └── draw_strategy.hpp       # OSD 绘制策略抽象接口
├── plugins/                    # 插件源码（参考实现）
│   ├── netclient_plugin/       # 输入：网络拉流（RTSP）
│   ├── persondet_plugin/       # 算法：行人检测（含区域规则）
│   ├── absence_plugin/         # 算法：离岗检测
│   ├── promptdet_plugin/       # 算法：开放词汇提示词检测
│   ├── facedet_plugin/         # 算法：人脸检测
│   ├── firedet_plugin/         # 算法：火灾/烟雾检测
│   ├── catdog_plugin/          # 算法：宠物检测
│   ├── netserver_plugin/       # 输出：网络推流（RTSP Push）
│   └── hdmi_plugin/            # 输出：HDMI 显示
├── example/                    # SDK 功能样例程序
├── doc/                        # 协议文档与配置参考
│   ├── ALGORITHM_BROCHURE_CN.md # 软件算法宣传彩页
│   ├── PLUGIN_CONFIG_REFERENCE_ZH.md
│   └── USER_MANUAL/
├── build_sdk_sample.sh         # 一键构建（样例 + 所有插件）
└── CMakeLists.txt
```

---

---

## 3. 内置算法组件一览

SDK 出厂内置以下算法插件（`.plugin` 包），可直接在 Web 端创建任务时选用：

### 3.1 视频输入组件

| 节点 type | 显示名 | 说明 |
| :-- | :-- | :-- |
| `netclient` | 网络拉流 | RTSP 拉流输入，支持按时布控（`schedule_config`） |

### 3.2 算法组件

> 以下所有算法插件均为官方内置，直接在 Web 端建任务时可选用，无需额外部署。

#### 🧍 人员安防类

| 插件 | 显示名 | 关键能力 |
| :-- | :-- | :-- |
| `intrusion` | 区域入侵检测 | 多边形区域进入/离开实时报警 |
| `loitering` | 徘徊检测 | 目标在区域内停留超时报警 |
| `absence` | 离岗检测 | 岗位区域无人或人员离开超时报警 |
| `crowd` | 人群聚集检测 | 区域内目标数量超阈值报警 |
| `tripwire` | 绊线方向检测 | 虚拟线越线 + 方向判断（支持正行/逆行） |
| `peopleflow` | 人流统计 | 进出计数 + 峰值超阈值报警 |
| `humanattr` | 行人属性检测 | 年龄/性别/着装/安全绳等 25 项属性 |
| `facedet` | 人脸检测 | 人脸定位 + 质量评分 + OSD |
| `facerec` | 人脸识别 | 人脸特征提取与比对（黑白名单） |

#### 🚗 车辆管理类

| 插件 | 显示名 | 关键能力 |
| :-- | :-- | :-- |
| `lprV2` | 车牌识别 | 车牌号识别 + 车型判断（V2 精度更高） |
| `illpark` | 违章停车检测 | 划定禁停区域，停车超时触发报警 |
| `vehicleflow` | 车流统计 | 车辆进出计数 + 峰值超阈值报警 |
| `wrongway` | 车辆逆行检测 | 指定方向行驶违规识别 |

#### 🔥 安全生产类

| 插件 | 显示名 | 关键能力 |
| :-- | :-- | :-- |
| `firesmoke` | 火灾烟雾检测 | 火焰/烟雾检测，支持 LLM 二次审核 |
| `helmet` | 安全帽检测 | 未佩戴安全帽实时报警 |

#### 🎭 行为识别类

| 插件 | 显示名 | 关键能力 |
| :-- | :-- | :-- |
| `fight` | 打架检测 | 肢体冲突行为识别 |
| `falldown` | 摔倒检测 | 人员跌倒姿态识别 |
| `calling` | 打电话检测 | 手持设备打电话行为识别 |
| `smoking` | 吸烟检测 | 吸烟行为识别 |

#### 🧠 开放词汇与智能审核类

| 插件 | 显示名 | 关键能力 |
| :-- | :-- | :-- |
| `promptdet` | 提示词检测 | 基于开放词汇提示词的通用目标检测，支持区域、规则、跟踪去重与报警推送 |
| `llm` | LLM 二次审核 | 调用外部大模型对告警帧进行语义校验，降低误报 |

#### 🔧 功能辅助类

| 插件 | 显示名 | 关键能力 |
| :-- | :-- | :-- |
| `osd` | OSD 叠加 | 字幕 / Logo / 时间水印叠加 |

### 3.3 视频输出组件

| 插件 | 显示名 | 说明 |
| :-- | :-- | :-- |
| `alarm` | 报警推送 | 统一报警路由（HTTP / TTS / 本地存储） |
| `record` | 录像存储 | 触发式抓图 / 录像落盘 |
| `netserver` | 网络推流 | RTSP Push 编码推流 |
| `hdmi` | HDMI 输出 | 带 OSD 叠加的本地 HDMI 显示 |
| `gb28181` | GB28181 | 国标视频联网协议输出 |
| `p2p` | P2P 直播 | P2P 实时预览推流 |

### 3.4 区域规则引擎说明

`persondet` 插件集成了 `RegionAnalyzer`，**同一个插件通过前端绘制不同形状和开关配置，可实现 6 种安防场景**：

```text
前端操作                   触发的规则类型
──────────────────────────────────────────────────────
绘制多边形 + 进入开关     →  区域入侵检测（Enter/Leave）
绘制多边形 + 徘徊开关     →  徘徊检测（loiter_sec 超时）
绘制多边形 + 离岗开关     →  离岗检测（absence_sec 无人超时）
绘制多边形 + 人群开关     →  人群聚集检测（crowd_threshold 人数阈值）
绘制两点连线              →  绊线方向检测（line_cross 越线方向）
不绘制区域                →  全画面行人检测
```

关键配置项（`config.json` 中设置）：

| 配置 key | 默认值 | 说明 |
| :-- | :-- | :-- |
| `region_enable_enter` | `true` | 开启进入/离开报警 |
| `region_enable_loiter` | `false` | 开启徘徊检测 |
| `region_loiter_sec` | `10` | 徘徊触发时间（秒） |
| `region_loiter_cooldown` | `30` | 徘徊报警冷却（秒） |
| `region_enable_absence` | `false` | 开启离岗检测 |
| `region_absence_sec` | `30` | 离岗触发时间（秒） |
| `region_enable_crowd` | `false` | 开启人群聚集 |
| `region_crowd_threshold` | `10` | 聚集触发人数 |
| `region_enable_line_cross` | `false` | 开启绊线越线 |

### 3.5 开放词汇提示词检测

`promptdet` 插件面向“类别经常变化、但不希望为每个场景重新训练模型”的业务。用户在 Web 端下发英文提示词，例如 `person`、`bus`、`traffic cone`、`trash bag`、`cardboard box`，系统会理解提示词语义，并在实时视频中检测对应目标。

核心能力：

- **动态提示词**：每个任务最多 5 个英文提示词；一行一个，提示词内部可以包含空格。
- **统一资源交付**：检测能力、提示词理解能力和必要资源随插件统一交付，插件根据运行平台自动选择合适的运行后端。
- **区域规则**：未绘制区域时默认全画面检测；绘制多边形后只对区域内目标生效。
- **通用事件规则**：支持目标出现、目标缺失、目标静止、新增物体、数量超限、动作候选等规则类型。
- **报警治理**：内置轻量跟踪、连续帧确认、重复报警间隔、本地网页报警和服务器报警推送，适合实时视频场景长期运行。

典型场景：

| 场景 | 推荐提示词 | 推荐规则 |
| :-- | :-- | :-- |
| 门口包裹检测 | `package` / `cardboard box` | 新增物体或目标出现 |
| 通道堆物检测 | `box` / `trash bag` / `carton` | 目标静止 |
| 道路异物检测 | `traffic cone` / `debris` | 目标出现或目标静止 |
| 岗位物品缺失 | `fire extinguisher` / `helmet` | 目标缺失 |
| 行为类候选触发 | `cigarette` / `smoke` | 动作候选，建议接二阶段审核 |

> 当前版本建议使用英文提示词。中文提示词不会报错，但语义对齐不稳定，不建议用于正式布控。

---

## 4. 插件开发与算法快速集成

## 4.1 插件生命周期（必须理解）

每个插件必须导出两个 C 接口：

```cpp
extern "C" void plugin_init(SDKInterface* sdk);
extern "C" void plugin_cleanup(SDKInterface* sdk);
```

运行时流程：

1. `PluginLoader` 扫描 `.plugin` 文件。
2. `plugin_init` 内部调用 `sdk->register_node(type, creator)`。
3. 卸载时调用 `plugin_cleanup`，执行 `sdk->unregister_node(type)`。

## 4.2 插件开发标准步骤

### 步骤 1：复制一个最接近的模板插件
推荐从 `persondet_plugin` / `firedet_plugin` / `netclient_plugin` 复制。

> 目录命名建议使用 `xxx_plugin`（以 `_plugin` 结尾）。当前 `plugins/CMakeLists.txt` 默认通过 `*_plugin` 模式自动发现子工程。

### 步骤 2：定义节点参数结构
在 `JdkXXXNode.hpp` 中定义 `NodeParams`，集中管理配置项默认值。

### 步骤 3：在 `plugin_infer.cpp` 解析配置
使用 `jp(config, key, default)` 读取参数，构造节点对象并注册：

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
若使用 AX 算法 SDK，优先使用 `SafeAlgorithm` 封装；若插件同时支持 RKNN，建议在构造阶段基于 `PluginRuntime` 选择后端：

```cpp
SafeAlgorithm::Options opt{ax_model_type_fire_smoke, nodeParams_->model_path, nodeParams_->runtime_device_id};
alg_ = std::make_shared<SafeAlgorithm>(opt);
alg_->set_affinity(true);
alg_->update_params([&](auto &p) {
    p.det_threshold = 0.8f;
});
```

RK / AX 双后端示例：

```cpp
// runtime.infer_type(): rk.local -> "rk"，ax.local / compute_card_N -> "ax"
infer_ = YOLOV5FACE::create_infer(model_path,
                                   runtime.infer_type(),
                                   runtime.runtime_device_id);
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

## 4.3 快速集成自有算法（建议路径）

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

## 4.4 推荐管线模板

### 人员检测场景
`netclient → persondet → alarm`

### 人脸识别场景
`netclient → facerec → alarm`（可加 `record`）

### 车辆抓拍场景
`netclient → lprV2 → alarm / gb28181`

### 消防安全场景
`netclient → firedet → alarm`（TTS 音柱播报）

### 多算法并联场景
```text
netclient ──┬──► persondet ──┐
            ├──► firedet   ──┼──► alarm
            └──► facedet   ──┘
```

## 4.5 开发注意事项
- 节点 `type` 必须与 `config.json` 中 `component.type` 一致，否则前端无法识别组件。
- `plugin_init` / `plugin_cleanup` 必须加 `extern "C"` 导出，否则出现 `undefined symbol: plugin_init`。
- `alarm_fn` 返回空 JSON 对象时，`alarm_plugin` 跳过该次推送——务必确保 `alarm_count > 0` 时才填充 `alarms[]`。
- `push_interval_ms` 在 `ResultEntry` 粒度生效，同一节点不同 track_id 共享同一冷却时钟。
- 多路场景用 `device_id` / `channel_id` 做资源隔离，避免 NPU 亲和性冲突。

---

## 5. 报警协议与报文规范

本章分两层：

- **层 A（插件内部协议）**：算法插件与 `alarm_plugin` 之间的 JSON 协议（由 `alarm_fn` 返回）。
- **层 B（设备外部协议）**：设备向平台 HTTP 上报协议（规范 V1.0.2）。

## 5.1 层 A：`alarm_fn` 统一报警结构

```json
{
  "msg": "TaskId:xxx 检测到目标",
  "alarm_type": "persondet_alarm",
  "timestamp": "2026-03-07T10:10:10.123Z",
  "alarm_count": 1,
  "alarm_push_uri": "/api/v1/device/report/event",
  "server_push_uri": "/api/v1/face/capture",
  "alarms": [
    {
      "local_push_msg": {
        "alarm_type": "persondet_alarm",
        "region_event": "Enter",
        "region_id": 0,
        "track_id": 42,
        "bbox": {"x": 100, "y": 120, "w": 200, "h": 260},
        "tts_text": "注意，检测到人员入侵"
      },
      "server_push_msg": {
        "time": 1770000000000,
        "sn": "device-sn",
        "event": "Enter",
        "face_data": "data:image/jpeg;base64,...",
        "bg_data":   "data:image/jpeg;base64,..."
      },
      "alarm_push_msg": {
        "did": "device-id",
        "type": 11,
        "time": 1770000000000,
        "state": 0,
        "data": {
          "event": "Enter",
          "region_id": 0,
          "track_id": 42,
          "bg_data": "data:image/jpeg;base64,..."
        }
      }
    }
  ]
}
```

`alarm_plugin` 处理规则：

| 字段 | 行为 |
|---|---|
| `server_push_uri` + `server_push_msg` | POST 到 `server_push_uri`（业务抓拍） |
| `alarm_push_uri` + `alarm_push_msg` | POST 到 `alarm_push_uri`（事件上报） |
| `local_push_msg` | 本地 UI / 数据库 / TTS，不对外 HTTP 推送 |

## 5.2 TTS 字段（放在 `local_push_msg` 中）

| 字段 | 说明 |
|---|---|
| `tts_text` | 直接播报文本 |
| `tts_url` | 播放远端音频 URL |
| `tts_queue` | `["文本1", "文本2"]` 队列顺序播报 |

## 5.3 `EventType` 映射（代码基准）

来源：`thirdpark/comm/include/DevProtoDef.hpp`

| 值 | 枚举名 | 含义 |
| :-- | :-- | :-- |
| 1 | MOTION_DETECTION | 移动侦测 |
| 2 | INTRUSION_DETECTION | 入侵 / 绊线 |
| 3 | VIDEO_BLIND | 视频遮挡 |
| 4 | BABY_CRYING | 婴儿哭声 |
| 5 | CROWD_GATHERING | 人群聚集 |
| 6 | PERSON_LOITERING | 人员徘徊 |
| 7 | FAST_MOVING | 快速移动 |
| 8 | FIRE_ALARM | 火焰 / 烟雾 |
| 9 | SOUND_SPIKE | 声音突增 |
| 10 | NOISE_DROP | 声音骤降 |
| 11 | HUMAN_DETECTION | 人形抓拍 |
| 12 | WHITELIST_DETECTION | 白名单识别 |
| 13 | BLACKLIST_DETECTION | 黑名单识别 |
| 14 | VIP_DETECTION | VIP 识别 |
| 15 | POST_ABANDONMENT | 离岗检测 |
| 16 | FALL_DETECTION | 摔倒检测 |
| 17 | VITAL_SIGN_DETECTION | 生命体征 |
| 18 | KITCHEN_MONITORING | 厨房监控 |
| 19 | BEHAVIOR_DETECTION | 行为检测（打架/吸烟/打电话） |
| 20 | NON_MOTORIZED_VEHICLE | 非机动车 |

> SDK 当前已扩展到 `type=20`，协议文档 V1.0.2 示例覆盖到 `14`。**建议后端枚举做前向兼容，未知值不要拒绝，改用 `type + alarm_type` 双字段联合识别。**

## 5.4 层 B：设备对外 HTTP 协议

根据 `平台设备对接协议规范 V1.0.2`：

| 接口 | 方法 | 用途 |
| :-- | :-- | :-- |
| `/api/v1/device/report/event` | POST | 通用事件上报 |
| `/api/v1/device/report/info` | POST | 设备信息上报 |
| `/api/v1/adapter/lenfocus/face/capture` | POST | 人脸抓拍上报 |
| `/api/v1/adapter/lenfocus/vehicle/capture` | POST | 车辆抓拍上报 |

**事件上报标准体：**

```json
{
  "did": "1234512",
  "type": 11,
  "time": 1666781577816,
  "data": {
    "face_data":       "data:image/jpeg;base64,...",
    "body_data":       "data:image/jpeg;base64,...",
    "bg_data":         "data:image/jpeg;base64,...",
    "jpeg_url_face":   "sdcard/capture/xxx_face.jpg",
    "jpeg_url_body":   "sdcard/capture/xxx_body.jpg",
    "jpeg_url_frame":  "sdcard/capture/xxx_frame.jpg"
  }
}
```

## 5.5 报警链路联调清单

- [ ] 算法插件输出 `alarm_count > 0`
- [ ] `alarm_push_uri` / `server_push_uri` 非空
- [ ] `alarm_plugin` 配置的 `http_url` 网络可达
- [ ] 报文包含 `did` / `type` / `time` 字段
- [ ] `data.bg_data` 图片大小未超限（过大导致接口超时）
- [ ] TTS：`tts_enabled=true` 且 `tts_speaker_ip` 可达

---

## 6. SDK Demo 说明

| Demo | 作用 | 关键类 |
| :-- | :-- | :-- |
| `jdk_frame_sample` | 帧对象加载 / 保存 | `AXVideoFrame` |
| `jdk_capture_sample` | NV12 / JPEG 抓拍 | `HwCapture` |
| `jdk_ivps_sample` | IVPS 图像处理（畸变矫正 / 色彩转换） | `HwIvps` |
| `jdk_npu_sample` | NPU 模型推理 + OSD 绘制 | `YOLOFACE` + `HwIvps` |
| `jdk_alg_sample` | `SafeAlgorithm` 统一接口（车牌识别演示） | `SafeAlgorithm` |
| `jdk_netclient_vdec_sample` | RTSP 拉流 / 取帧 | `NetClient` |
| `jdk_netserver_venc_sample` | 编码推流 | `HwEncoder` |
| `jdk_node_sample` | 动态加载插件并输出组件结构 | `PluginLoader` + `NodeFactory` |

**建议验证顺序：**
1. `jdk_frame_sample` → 验证文件读写
2. `jdk_capture_sample` / `jdk_ivps_sample` → 验证图像处理链
3. `jdk_npu_sample` / `jdk_alg_sample` → 验证推理栈
4. `jdk_node_sample` + 插件 → 验证插件加载与 UI 配置展示

---

## 7. 插件配置文件使用说明

## 7.1 配置文件分层

```text
config.json（由 config_template.json.in 生成）
├── name / version / platform / entry / md5 / type
├── model_path / model_files[]
└── component
    ├── parentType     # input-component / algorithm-component / output-component
    ├── type           # 必须与 register_node() 注册名一致
    ├── label          # { "zh": "...", "en": "..." }
    └── formList[]     # 前端表单控件定义
```

## 7.2 `formList` 常用控件类型

详见：[插件配置完整参考](reference/plugin-config-full.md)

| 类型 | 用途 |
|---|---|
| `input` / `inputNumber` / `password` / `textarea` | 文本 / 数字输入 |
| `select` / `switch` / `slider` | 下拉 / 开关 / 滑块 |
| `button` / `divider` / `subdivider` | 操作按钮 / 分割线 |
| `schedule` | 按时段布控配置 |
| `regionDraw` | 前端区域 / 绊线绘制（与 RegionAnalyzer 联动） |
| `readOnly` / `status` | 只读展示 / 运行状态 |

## 7.3 典型配置范式

```jsonc
// 算法插件示例（firedet）
{
  "threshold": 60,            // 检测阈值 0-100
  "alarm_push_enable": true,
  "alarm_push_interval": 5,   // 报警冷却（秒）
  "record_pic_type": 1,       // 落盘图片类型位掩码
  "llm_review_enable": false, // LLM 二次审核
  "alarm_relay_enable": false // 继电器联动
}
```

## 7.4 配置变更原则
1. 新增字段必须有默认值（`jp(config, key, default)`）。
2. 老字段尽量不重命名，否则需要数据库迁移。
3. 涉及后端解析的字段变更必须同步协议版本。

---

## 8. SDK 关键接口说明

## 8.1 `SDKInterface`（插件入口能力）

```cpp
struct SDKInterface {
    // 注册 / 注销节点类型
    std::function<void(const std::string&, NodeCreator)> register_node;
    std::function<void(const std::string&)>              unregister_node;
    // 日志（level: info / debug / warn / error）
    std::function<void(const std::string& level, const std::string& message)> log;
    // 读取全局配置（key-value）
    std::function<std::string(const std::string& key)> get_config;
    // 向宿主发布事件
    std::function<void(const std::string& topic, const std::string& payload)> publish_event;
};
```

## 8.2 `SafeAlgorithm`（推荐推理封装）

```cpp
// 初始化
SafeAlgorithm::Options opt;
opt.model_type = ax_model_type_person_detection;
opt.model_path = "./models/person.model";
opt.device_id  = -1;  // -1=本机, >0=算力卡编号
auto alg = std::make_shared<SafeAlgorithm>(opt);
alg->set_affinity(true);       // 随机 NPU 亲和性（多路场景必选）
alg->update_params([](auto& p) {
    p.det_threshold = 0.6f;
});

// 推理
ax_result_t result{};
alg->track(frame, result, Capture_);   // 检测 + 跟踪
// alg->detect(frame, result, Capture_); // 仅检测，不维持 track_id

// 属性扩展
ax_body_attr_t attr{};
alg->get_body_attr(frame, &result.objects[i].bbox, &attr, Capture_);

// 人脸特征
float feat[AX_ALGORITHM_FACE_FEATURE_LEN];
alg->get_face_feature_2(frame, &result.objects[i], feat, Capture_);

// 车牌识别
std::string plate = alg->get_plate(image);
```

**支持的模型类型（`ax_model_type_e`）：**

| 枚举值 | 模型能力 |
|---|---|
| `ax_model_type_person_detection` | 行人检测 + 跟踪 |
| `ax_model_type_person_attr` | 行人属性（25 项） |
| `ax_model_type_lpr` | 车牌识别 + 车型 |
| `ax_model_type_face_detection` | 人脸检测 + 关键点 |
| `ax_model_type_face_recognition` | 人脸特征提取 |
| `ax_model_type_face_attr` | 人脸属性（年龄/性别/表情/种族） |
| `ax_model_type_fire_smoke` | 火焰 / 烟雾检测 |
| `ax_model_type_cat_dog` | 宠物检测（猫/狗） |
| `ax_model_type_violence` | 打架行为检测 |
| `ax_model_type_motor` | 非机动车检测 |

## 8.3 `RegionAnalyzer`（区域规则引擎）

```cpp
#include "RegionAnalyzer.hpp"

region::RegionAnalyzer analyzer;
region::RegionRule rule;
rule.enable_enter  = true;
rule.enable_loiter = true;
rule.loiter_sec    = 10.f;

// 多边形区域（≥3点）
analyzer.add_region(region::Region{points}, rule);
// 虚拟绊线（2点）
analyzer.add_tripwire(region::Tripwire{p1, p2}, rule);

analyzer.set_track_timeout(10.f);
analyzer.set_callback([](const region::RegionEvent& ev) {
    // ev.type: Enter / Leave / Loiter / Absence / Crowd / LineCrossIn / LineCrossOut
    // ev.track_id / ev.region_id / ev.dwell_sec
});

// 每帧调用
analyzer.update(det_result, timestamp_ms);
```

## 8.4 `PluginLoader`（插件管理）

```cpp
PluginLoader loader;
loader.load_all_plugins("/usr/local/aibox/plugins", &sdk_interface);

// 热重载单个插件
loader.reloadSingle("persondet");

// 查询组件结构（供前端渲染）
nlohmann::json comp = loader.get_component_structure();
```

## 8.5 `jdk_frame_meta::result_map_`（结果总线）

```cpp
// 算法节点写入
auto entry = std::make_shared<jdk_objects::ResultEntry>();
entry->result        = std::make_shared<std::any>(my_result);
entry->render_fn     = [](const std::any& r, auto& canvas, auto ivps){ /* OSD */ };
entry->alarm_fn      = [](const std::any& r) -> AlarmPayload { /* 构造 JSON */ };
entry->push_enabled  = true;
entry->push_interval_ms = 5000;
meta->result_map_[node_name()].exchange(entry);

// 下游节点读取
auto entry = meta->result_map_["persondet"].load();
if (entry) { /* 使用 entry->result */ }
```


## 8.6 `PluginRuntime`（运行位置标准化）

TaskManager 会在创建节点时向插件配置注入标准运行时字段：

```json
{
  "runtime_location": "rk.local",
  "device_id": -1,
  "runtime_device_id": -1
}
```

插件应通过 `PluginRuntime::from_task_config(config)` 统一解析，不建议直接读取旧式 `device_id` 做平台判断。

```cpp
const auto runtime = PluginRuntime::from_task_config(config);

if (runtime.is_rk_local()) {
    // RK 本地：RK MPP / RGA / RKNN，runtime.runtime_device_id == -1
} else if (runtime.is_ax_local()) {
    // AX 本地：AX VDEC / IVPS / NPU / VENC，runtime.runtime_device_id == -1
} else if (runtime.is_compute_card()) {
    // AXCL 计算卡：runtime.runtime_device_id 为卡索引，从 0 开始
}

const char* backend = runtime.infer_type(); // rk.local -> "rk"，其它 AX 路径 -> "ax"
```

运行位置与资源隔离规则：

- `rk.local` 与 `ax.local` 均为本地资源，`runtime_device_id=-1`。
- `compute_card_1` 对应 `runtime_device_id=0`，`compute_card_2` 对应 `1`，以此类推。
- 解码、编码和图像处理通道按 `runtime_location` 独立分配，避免本地与计算卡资源互相覆盖。
- 普通 Host 没有本地媒体/NPU 后端时，前端不会开放本地运行位置；插件也不应把 `local` 当作可推理后端。

---

## 9. 编译、部署与运行

## 9.1 编译环境要求

| 项目 | 要求 |
|---|---|
| 主机 OS | Ubuntu 20.04 / 22.04 |
| 交叉编译器 | `aarch64-none-linux-gnu-g++`（12.2.rel1） |
| CMake | ≥ 3.10 |
| 工具链下载 | [百度网盘](https://pan.baidu.com/s/18CczjjNDnMhM15VDcAJcpQ?pwd=v8me)（提取码：`v8me`）<br>[谷歌网盘](https://drive.google.com/drive/folders/15cmvIBABTxfgwNvJvhgTI9tMyAiH8vVT?usp=drive_link) |

## 9.2 一键构建

```bash
cd aibox_sdk
bash build_sdk_sample.sh
```

脚本自动完成：

1. 配置交叉工具链 `PATH`
2. 编译根工程（所有 demo 样例）
3. 编译 `plugins/` 子工程并打包为 `.plugin`

## 9.3 手动构建（推荐 CI 使用）

```bash
export PATH="/home/work/ax/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-linux-gnu/bin:$PATH"

# 编译 demo 样例
cmake -B build && cmake --build build -j$(nproc)

# 编译并打包插件
cmake -S plugins -B plugins/build && cmake --build plugins/build -j$(nproc)
```

**产物：**
- 样例程序：`build/example/<demo_name>`
- 插件包：`plugins/build_out/*.plugin`

## 9.4 板端部署

```bash
# 1. 安装运行时（板端执行）
dpkg -i aibox-runtime_*.deb

# 2. 部署插件
cp plugins/build_out/*.plugin /usr/local/aibox/plugins/

# 3. 设置库路径（可写入 /etc/profile.d/）
export LD_LIBRARY_PATH=/usr/local/aibox/lib:$LD_LIBRARY_PATH

# 4. 重启服务
service aibox restart

# 5. 查看实时日志
journalctl -u aibox -f
```

## 9.5 快速验收清单
1. `jdk_node_sample` 输出各插件组件结构 → 确认插件加载成功
2. Web 端新建任务，配置表单正常渲染
3. 触发目标事件，确认：
   - 本地告警（Web / App 通知）
   - HTTP 上报（`/api/v1/device/report/event`）
   - 图像落盘（`sdcard/capture/`）
   - TTS 播报（若启用）


## 9.6 RK 本地运行环境

当设备运行位置选择 `rk.local` 时，需要板端运行环境包含 Rockchip 相关运行库和驱动：

| 能力 | 依赖 | 用途 |
|---|---|---|
| 视频解码 / 编码 | RK MPP | RTSP H.264/H.265 拉流解码、编码输出、WebRTC 预览 |
| 图像处理 | librga / RGA 驱动 | OSD 合成、缩放、裁剪、格式转换、仿射变换 |
| NPU 推理 | RKNN Runtime | `.rknn` 模型加载与推理 |
| 帧内存 | dma-buf / MPP buffer | 在 MPP、RGA、RKNN 之间减少大帧拷贝 |

建议部署检查：

```bash
# 1. 确认 RK 设备节点和驱动可用
ls /dev/rga /dev/mpp_service 2>/dev/null

# 2. 确认运行库可被加载
ldd /usr/local/aibox/bin/TaskManager | grep -E 'rga|mpp|rknn'

# 3. 启动服务前设置运行库路径
export LD_LIBRARY_PATH=/usr/local/aibox/lib:$LD_LIBRARY_PATH
```

RK 本地开发注意事项：

- 推理模型必须提供 RKNN 格式，建议使用 `model_path_rk` 与 AX 模型分开配置。
- 对实时 MPP / RGA 帧，优先沿用 `AXVideoFrame` 抽象继续传递；不要为了普通处理流程频繁 `toHost()`，否则会引入缓存同步和大内存拷贝。
- 需要保存调试图、业务抓拍或 CPU 算法输入时，再使用保存接口或显式 Host 同步。
- 多任务场景下，RGA / RKNN 由运行时调度，不建议插件自行创建全局单例硬件上下文。

---

## 10. 常见问题与排障

### 10.1 `undefined symbol: plugin_init`
**原因：** 未加 `extern "C"` 或函数名拼写错误。  
**修复：** 确认插件入口为：
```cpp
extern "C" void plugin_init(SDKInterface* sdk) { ... }
extern "C" void plugin_cleanup(SDKInterface* sdk) { ... }
```

### 10.2 模型加载失败 (`ax_error_code_init_model_fail`)
检查：

- `config.json` 中 `model_files[]` 路径是否与包内文件名一致
- `.plugin` 包内是否实际包含模型文件
- 运行时目录是否有读权限（`ls -la /usr/local/aibox/plugins/`）

### 10.3 前端表单不显示
检查：

- `component.parentType` 是否为 `algorithm-component` / `input-component` / `output-component`
- `component.type` 是否与 `register_node()` 注册的 `type` 完全一致（区分大小写）
- `formList` 中每个控件是否有 `type` / `key` / `name` 三个必填字段

### 10.4 报警上报失败
检查：

- `alarm_plugin` 的 `http_url` 指向正确的后端地址
- `alarm_push_uri` 拼接后 URL 是否可达（`curl -X POST <url>`）
- 报文中 `did` / `type` / `time` 字段是否存在
- `bg_data` base64 图片是否过大（建议压缩到 200KB 以内）

### 10.5 TTS 不播报
检查：

- `tts_enabled=true`（`alarm_plugin` 配置）
- `tts_speaker_ip` 地址可 ping 通
- `alarm_fn` 返回的 `local_push_msg` 中包含 `tts_text` / `tts_url` / `tts_queue` 之一

### 10.6 调试辅助环境变量

| 变量 | 说明 |
|---|---|
| `AXPLUGIN_KEEP_TMP_SO=1` | 保留解压后的临时 `.so`，方便 GDB 调试 |
| `PLUGIN_DECRYPT_KEY=<key>` | 加密插件解密密钥 |
| `ALARM_SNAPSHOT_RETENTION_DAYS=7` | 快照保留天数（默认 7） |
| `ALARM_SNAPSHOT_MAX_BYTES_MB=500` | 快照占用上限（MB，默认 500） |
| `AIBOX_RK_IVPS_DIAG=1` | 打开 RK RGA / IVPS 诊断日志，定位缩放、OSD、格式转换问题 |
| `AIBOX_RKNN_DMA_INPUT=1` | RKNN 输入尝试 dma-buf 绑定路径，需确认模型输入格式与 RKNN Runtime 支持情况 |

### 10.7 `rk.local` 不出现在运行位置列表
检查：

- 当前设备是否为 RK / RV / Rockchip 平台（`cat /proc/device-tree/compatible`）。
- RK 运行库是否随固件安装完整。
- 普通 x86 / 树莓派 Host 仅在插入 AXCL 计算卡时显示 `compute_card_N`，不会显示 `rk.local` 或 `ax.local`。

### 10.8 RK 本地视频正常但推理异常
检查：

- 插件是否从 `PluginRuntime` 获取 `runtime_location`，并在 `rk.local` 下加载 `.rknn` 模型。
- 输入帧格式是否与模型预处理一致（常见为 NV12 -> resize/letterbox -> RGB/NHWC）。
- 是否在实时链路中无必要地调用 `toHost()`，导致缓存同步时机或性能异常。
- 如需排查 RGA 路径，可临时设置 `AIBOX_RK_IVPS_DIAG=1`，定位后关闭。

---

## 11. 其它工程化建议

## 11.1 发布前检查清单
- [ ] `config_template.json.in` 字段与代码读取键一致
- [ ] `plugin_init` / `plugin_cleanup` 成对注册注销
- [ ] `alarm_fn` 返回结构符合第 5 章规范
- [ ] 压测 `push_interval_ms` 与快照空间策略
- [ ] 用真实后端完成一次完整联调（含 TTS / HTTP / 落盘）

## 11.2 性能建议
- 合理设置 `det_threshold`（推荐 0.5–0.7）和 `push_interval_ms`（推荐 5000ms），避免过载告警。
- 多路场景优先通过 Web 端选择 `runtime_location` 做资源隔离：AX 本地使用 `ax.local`，RK 本地使用 `rk.local`，计算卡使用 `compute_card_N`。
- 控制上报图片类型（`record_pic_type` 位掩码），优先只上报 `bg_data`，减少带宽。
- LLM 二次审核（`llm_review_enable`）会增加延迟，仅在误报率高的场景启用。
- RK 本地链路应尽量保持 MPP/RGA/RKNN 闭环，避免在每帧上做 NV12/RGB 大块 CPU 拷贝。

---

## 附录 A：最小插件骨架

```cpp
// my_plugin/plugin_infer.cpp
#include "sdk_interface.hpp"
#include "jdk_node_wrapper.hpp"
#include "alg_comm.hpp"

namespace jdk_nodes {

struct MyParams {
    float       threshold{0.6f};
    int         device_id{-1};
    std::string model_path{"./models/my_model.model"};
    std::string task_id{"0"};
};

class MyNode : public jdk_node_base,
               public CustomHandleFrame,
               public CustomHandleControl {
public:
    MyNode(std::string name, std::unique_ptr<MyParams> p)
        : jdk_node_base(std::move(name)), params_(std::move(p)) {
        SafeAlgorithm::Options opt{ax_model_type_person_detection,
                                   params_->model_path, params_->device_id};
        alg_ = std::make_shared<SafeAlgorithm>(opt);
        alg_->set_affinity(true);
        alg_->update_params([&](auto& p){ p.det_threshold = params_->threshold; });
    }

protected:
    void run_infer_combinations(
        const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& batch) override
    {
        for (auto& meta : batch) {
            ax_result_t det{};
            alg_->track(meta->frame, det, Capture_);

            auto entry = std::make_shared<jdk_objects::ResultEntry>();
            entry->result = std::make_shared<std::any>(det);
            entry->alarm_fn = [this](const std::any& r) -> AlarmPayload {
                const auto& res = std::any_cast<const ax_result_t&>(r);
                if (res.n_objects == 0) return {};
                nlohmann::json root;
                root["alarm_type"]     = "my_alarm";
                root["alarm_count"]    = res.n_objects;
                root["alarm_push_uri"] = "/api/v1/device/report/event";
                nlohmann::json alarm;
                alarm["alarm_push_msg"]["did"]   = params_->task_id;
                alarm["alarm_push_msg"]["type"]  = 11;
                alarm["alarm_push_msg"]["time"]  = now_ms();
                alarm["alarm_push_msg"]["state"] = 0;
                root["alarms"].push_back(alarm);
                return {root, {}};
            };
            entry->push_enabled     = true;
            entry->push_interval_ms = 5000;
            meta->result_map_[node_name()].exchange(entry);
        }
    }

    std::shared_ptr<jdk_objects::jdk_meta>
    handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) override final {
        return jdk_node_base::handle_frame_meta(meta);
    }
    std::shared_ptr<jdk_objects::jdk_meta>
    handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) override {
        return meta;
    }

private:
    std::unique_ptr<MyParams>       params_;
    std::shared_ptr<SafeAlgorithm>  alg_;
};

} // namespace jdk_nodes

extern "C" void plugin_init(SDKInterface* sdk) {
    sdk->register_node("my_plugin", [](const std::string& name, const nlohmann::json& cfg) {
        auto p = std::make_unique<jdk_nodes::MyParams>();
        p->threshold  = std::clamp(jp(cfg, "threshold", 60), 0, 100) / 100.0f;
        p->device_id  = jp(cfg, "device_id", -1);
        p->model_path = jp(cfg, "model_path", std::string("./models/my_model.model"));
        p->task_id    = jp(cfg, "task_id", std::string("0"));
        return jdk_nodes::jdk_node_wrapper::create(
            name, std::make_shared<jdk_nodes::MyNode>(name, std::move(p)));
    });
}

extern "C" void plugin_cleanup(SDKInterface* sdk) {
    sdk->unregister_node("my_plugin");
}
```

---

## 附录 B：参考文档
- [软件算法宣传彩页](reference/algorithm-brochure.md)
- [用户使用手册](user-manual/index.md)
- [插件使用说明](reference/plugin-config-full.md)
- [平台设备对接协议规范-V1.0.2.docx](../assets/downloads/platform-device-integration-protocol-v1.0.2.docx)
- [文本TTS与媒体URL播放接口.pdf](../assets/downloads/text-tts-media-url-api.pdf)

---

© AIBox SDK Team
