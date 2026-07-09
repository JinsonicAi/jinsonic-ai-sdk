# 常见问题 FAQ

本页汇总开发、部署和现场运维中的高频问题。按错误现象快速定位。

## 插件加载

### `undefined symbol: plugin_init`

**原因**：未加 `extern "C"` 或函数名拼写错误。

**修复**：确认插件入口为：

```cpp
extern "C" void plugin_init(SDKInterface* sdk) { ... }
extern "C" void plugin_cleanup(SDKInterface* sdk) { ... }
```

### 模型加载失败（`ax_error_code_init_model_fail`）

检查：

- `config.json` 中 `model_files[]` 路径是否与包内文件名一致。
- `.plugin` 包内是否实际包含模型文件。
- 运行时目录是否有读权限（`ls -la /usr/local/aibox/plugins/`）。

### 前端表单不显示

检查：

- `component.parentType` 是否为 `algorithm-component` / `input-component` / `output-component`。
- `component.type` 是否与 `register_node()` 注册的 `type` **完全一致**（区分大小写）。
- `formList` 中每个控件是否有 `type` / `key` / `name` 三个必填字段。

## 报警链路

### 视频能播放但没有报警

检查：

- 算法阈值是否过高。
- 区域或绊线是否配置正确。
- 报警插件是否接在算法节点后面。
- `alarm_count` 是否大于 0。

### 报警有 OSD 但平台没有收到

检查：

- 报警推送节点是否启用。
- `alarm_plugin` 的 `http_url` 是否指向正确的后端地址。
- `alarm_push_uri` 拼接后的 URL 是否可达（`curl -X POST <url>`）。
- 报文中 `did` / `type` / `time` 字段是否存在。
- `bg_data` base64 图片是否过大（建议压缩到 200KB 以内）。
- 报警冷却是否正在生效。

### TTS 不播报

检查：

- `tts_enabled=true`（`alarm_plugin` 配置）。
- `tts_speaker_ip` 地址可 ping 通。
- `alarm_fn` 返回的 `local_push_msg` 中包含 `tts_text` / `tts_url` / `tts_queue` 之一。

### 485 继电器不动作

检查：

- `alarm` 节点是否启用 485 继电器。
- `relay_device` 是否对应真实串口（`/dev/ttyS1`、`/dev/ttyUSB0`）。
- 波特率、从站地址、通道和线圈地址是否与继电器板一致。
- 内核是否已启用 RS485 模式。
- 串口是否被其它进程占用。

## 运行位置

### `rk.local` 不出现在运行位置列表

检查当前设备是否为 RK / RV / Rockchip 平台：

```bash
cat /proc/device-tree/compatible
```

非 RK 平台不会开放本地运行位置，只能使用外挂 AXCL 算力卡（`compute_card_N`）。

### RK 本地运行报错找不到库

确认 RK 运行库可被加载：

```bash
ldd /usr/local/aibox/bin/TaskManager | grep -E 'rga|mpp|rknn'
export LD_LIBRARY_PATH=/usr/local/aibox/lib:$LD_LIBRARY_PATH
```

## 编译构建

### 找不到交叉编译器

确认工具链已解压且 `PATH` 已设置：

```bash
export PATH="/home/work/ax/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-linux-gnu/bin:$PATH"
aarch64-none-linux-gnu-g++ --version
```

工具链下载地址见 [快速开始 › 准备交叉编译环境](quickstart.md#2-准备交叉编译环境)。

### 插件没有被打包

确认插件目录以 `_plugin` 结尾。`plugins/CMakeLists.txt` 默认通过 `*_plugin` 模式自动发现子工程。

## 调试环境变量

| 变量 | 说明 |
|---|---|
| `AXPLUGIN_KEEP_TMP_SO=1` | 保留解压后的临时 `.so`，方便 GDB 调试 |
| `AIBOX_RK_IVPS_DIAG=1` | 打开 RK RGA / IVPS 诊断日志 |
| `AIBOX_RKNN_DMA_INPUT=1` | RKNN 输入尝试 dma-buf 绑定路径 |

更多变量见 [交付运维 › 调试辅助环境变量](deployment-ops.md)。

## 还没解决？

- 完整排障章节：[SDK 开发指南](sdk-guide.md)
- 报警链路联调：[报警联动](alarm-linkage.md)
- 部署运维流程：[交付运维](deployment-ops.md)
