# 更新记录

本页记录文档站和 SDK 的主要版本变化。

!!! note "版本基线"
    当前文档对应 SDK `aibox_sdk`、固件基线 `3.10.2`、平台设备对接协议规范 `V1.0.2`（2026-02-21）。

## 文档站

### 2026-09-06（OpenAPI 文档与示例 1.4.9）

- [协议](reference/openapi-protocol.md)同步用户名密码自助领取 Client 凭据，后续仍使用原 Client Credentials / Token / 业务接口。
- 增加[中英文调用步骤](reference/openapi-examples.md)和 Python / Node.js / curl 示例 ZIP 下载及 SHA-256。
- 明确改密失效、Secret 只返回一次、WSS 有限退避、证书要求及[已验证/未覆盖边界](reference/openapi-validation.md)。

### 2026-07（内容优化）

- 首页改版：卡片式角色导览、架构图和文档地图。
- 快速开始：补充工具链下载地址、逐步编译/部署命令、产物路径和分层验收。
- 运行位置与部署：新增选型决策图、RK 本地运行环境和资源隔离规则。
- 插件开发：补充从零开发标准 10 步、节点生命周期时序图和配置控件表。
- 报警联动：补充两层协议、`alarm_fn` 完整报文、EventType 映射和联调清单。
- 算法能力：补充完整算法矩阵、区域规则引擎和行业方案组合。
- 交付运维：补充监控指标详解和调试环境变量。
- 新增 [常见问题 FAQ](faq.md) 页。

## SDK 能力

### 多运行位置与 RK 本地支持

- 统一 Runtime Location 抽象：`ax.local` / `rk.local` / `compute_card_N`。
- RK 本地支持 MPP 编解码、RGA 图像处理和 RKNN 推理。
- 插件通过 `PluginRuntime::from_task_config()` 统一解析运行位置，无需写死平台分支。

### 开放词汇提示词检测

- 新增 `promptdet` 插件，支持英文提示词定义关注目标。
- 每任务最多 5 个提示词，支持区域规则、连续帧确认和报警治理。
- 模型、文本编码器和词表统一打包为 `promptdet_model.axpkg`。

### 报警协议

- 协议扩展到 `EventType=20`（非机动车检测）。
- 建议后端做前向兼容，使用 `type + alarm_type` 双字段联合识别。

## 维护约定

- 协议或用户手册有新版本时，同步更新 [原始资料下载](reference/downloads.md) 附件和在线文档。
- 新增字段必须有默认值，老字段尽量不重命名，避免数据库迁移。
- 涉及后端解析的字段变更必须同步协议版本。
