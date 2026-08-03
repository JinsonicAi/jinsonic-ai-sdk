# AIBox 第三方开放 API 对接协议

| 文档属性 | 内容 |
|---|---|
| 文档名称 | AIBox 第三方开放 API 对接协议 |
| 文档版本 | `1.4.3` |
| 协议版本 | `OpenAPI v1` |
| 发布日期 | `2026-08-03` |
| 适用对象 | 第三方平台、客户自研 Web/服务端应用 |

> **开放边界**：本文只说明客户如何连接设备、发送请求和处理响应。AIBox 内部网页接口、数据库和插件文件不属于开放协议。

> **传输边界**：本文是“设备直连分册”，适用于客户系统能够通过局域网、专网、VPN 或私有 APN 访问盒子的场景。盒子位于 NAT/4G/普通互联网后时，应由盒子主动建立到 AIBox Device Gateway 的 WSS 长连接；客户平台调用 Gateway 北向 API，不得把设备 `8099` 端口直接暴露到公网。设备上行协议、Gateway API 与本协议共享业务 URI 和数据模型，但使用独立的设备身份、租户授权和连接会话。

## 文档导航

| 阅读目标 | 对应章节 |
|---|---|
| 完成首次连接和鉴权 | 第 1～2 章 |
| 了解 HTTP/WSS 报文格式 | 第 3～4 章 |
| 创建、编辑、启停任务 | 第 5 章 |
| 接收任务、设备、插件、告警和录像事件 | 第 6～7 章 |
| 查询全部 API | 第 8 章 |
| 处理分页、重试和错误 | 第 9～10 章 |
| 安全和版本兼容要求 | 第 11～12 章 |

## 1. 接入信息

| 项目 | 值 |
|---|---|
| HTTPS 地址 | `https://<设备IP>:8099/openapi/v1/command` |
| WSS 地址 | `wss://<设备IP>:8099/openapi/v1/ws?ticket=<ticket>` |
| 请求格式 | UTF-8 JSON |
| HTTP 方法 | `POST` |
| 鉴权请求头 | `X-Access-Token: <access_token>` |
| 单个 HTTP/WSS 消息上限 | `4 MiB` |
| Access Token 有效期 | 默认 `3600` 秒 |

所有 HTTPS 业务调用都发送到 `/openapi/v1/command`。文档中的其他 `/openapi/v1/...` 是请求 JSON 的 `uri`（下文称 **Command URI**），不是可直接访问的 HTTP Path。历史内部兼容路径不属于第三方协议。例如：

```http
POST /openapi/v1/command HTTP/1.1
Content-Type: application/json
X-Access-Token: <access_token>

{"uri":"/openapi/v1/device/get","param":{}}
```

### 1.1 部署方式

| 部署方式 | 连接方向 | 正式通道 | 适用场景 |
|---|---|---|---|
| 设备直连 | 客户系统 → 盒子 | HTTPS + WSS | 局域网、VPN、专网、私有 APN |
| 客户云管理 | 盒子 → Device Gateway | WSS 上行；客户平台 → Gateway HTTPS/WSS/Webhook | NAT、4G/5G、互联网和多设备集中管理 |

客户自研浏览器不得保存 `client_secret`。浏览器应登录客户自己的 BFF/Gateway，由服务端持有第三方凭证。当前 Client Credentials 流程面向可信服务端，不是浏览器公开客户端授权流程。

交付方会为每个第三方系统单独提供：

```text
client_id     公开标识，例如 aibc_xxx
client_secret 私密密钥，例如 aibsk_xxx，仅展示一次
```

`client_secret` 只用于换取短期 Access Token，不得放入 URL、网页前端代码或普通日志。

第三方 `openapi_client` Access Token 只能放在 `X-Access-Token`。URL Query、Cookie、JSON 顶层 `token` 和 `param.access_token` 均不属于 OpenAPI 鉴权方式。设备内部旧网页的兼容 Token 解析不构成对外协议。

## 2. 五分钟完成首次调用

### 2.1 获取 Access Token

请求不携带 `X-Access-Token`：

```bash
curl -X POST "https://<设备IP>:8099/openapi/v1/command" \
  -H "Content-Type: application/json" \
  -d '{
    "uri":"/openapi/v1/auth/token",
    "request_id":"req-auth-001",
    "param":{
      "client_id":"<client_id>",
      "client_secret":"<client_secret>"
    }
  }'
```

成功响应：

```json
{
  "uri": "/openapi/v1/auth/token",
  "code": 0,
  "msg": "OK",
  "request_id": "req-auth-001",
  "server_time_ms": 1785600000000,
  "result": {
    "access_token": "aibat_xxx",
    "token_type": "opaque",
    "token_use": "openapi_client",
    "expires_in": 3600,
    "expires_at": 1785603600,
    "client_id": "aibc_xxx",
    "scopes": ["device.read", "task.read", "task.write", "task.execute"]
  }
}
```

失败响应：

```json
{
  "code": 40101,
  "msg": "INVALID_CLIENT_CREDENTIALS",
  "result": {
    "error": {
      "category": "authentication",
      "retryable": false,
      "retry_after_ms": 0
    }
  }
}
```

连续失败会临时锁定该客户端并返回 HTTP `429`。客户端必须等待 `retry_after_ms` 后再试。

### 2.2 验证会话

```bash
curl -X POST "https://<设备IP>:8099/openapi/v1/command" \
  -H "Content-Type: application/json" \
  -H "X-Access-Token: <access_token>" \
  -d '{"uri":"/openapi/v1/session/get","param":{}}'
```

响应：

```json
{
  "code": 0,
  "msg": "OK",
  "result": {
    "subject": "client:aibc_xxx",
    "client_id": "aibc_xxx",
    "token_use": "openapi_client",
    "scopes": ["device.read", "task.read", "task.write", "task.execute"],
    "expires_at": 1785603600
  }
}
```

### 2.3 查询设备和任务

登录成功后建议依次调用：

```text
/openapi/v1/device/get
/openapi/v1/device/capabilities
/openapi/v1/nodes/catalog
/openapi/v1/tasks/list
```

查询任务示例：

```json
{
  "uri": "/openapi/v1/tasks/list",
  "request_id": "req-task-list-001",
  "param": {"page_size": 50}
}
```

响应：

```json
{
  "code": 0,
  "msg": "OK",
  "request_id": "req-task-list-001",
  "result": {
    "items": [],
    "total": 0,
    "has_more": false,
    "next_cursor": ""
  }
}
```

## 3. HTTP 请求与响应格式

### 3.1 请求

```json
{
  "uri": "/openapi/v1/tasks/get",
  "request_id": "req-unique-001",
  "idempotency_key": "write-request-unique-key",
  "param": {"task_id": "task-001"}
}
```

| 字段 | 必须 | 说明 |
|---|---|---|
| `uri` | 是 | API 路径 |
| `param` | 是 | 业务参数，无参数时传 `{}` |
| `request_id` | 建议 | 客户端生成的请求追踪 ID |
| `idempotency_key` | 写接口必须 | 同一次业务重试必须保持不变，1～128 个可打印 ASCII 字符 |

### 3.2 成功响应

```json
{
  "uri": "/openapi/v1/tasks/get",
  "code": 0,
  "msg": "OK",
  "request_id": "req-unique-001",
  "server_time_ms": 1785600000000,
  "result": {}
}
```

### 3.3 失败响应

```json
{
  "code": 40001,
  "msg": "INVALID_ARGUMENT",
  "result": {
    "error": {
      "category": "argument",
      "field": "task_id",
      "detail": "task_id is required",
      "retryable": false
    }
  }
}
```

客户端必须同时判断 HTTP Status 和 JSON `code`。只有 `code=0` 才表示业务成功。HTTPS 响应同时返回 `X-Request-ID`；OpenAPI 响应使用 `Cache-Control: no-store`，不得由浏览器或代理缓存。

## 4. WSS 连接与调用

WSS 用于低延时请求和实时事件。登录、申请 WSS Ticket、录像下载仍使用 HTTPS。

### 4.1 申请一次性 Ticket

请求头携带 `X-Access-Token`：

```json
{
  "uri": "/openapi/v1/ws/ticket",
  "request_id": "req-ws-ticket-001",
  "param": {}
}
```

响应：

```json
{
  "code": 0,
  "msg": "OK",
  "result": {
    "ticket": "<64位十六进制字符串>",
    "expires_at_ms": 1785600060000,
    "websocket_path": "/openapi/v1/ws",
    "protocol": "aibox.openapi.v1",
    "max_message_bytes": 4194304,
    "heartbeat_interval_ms": 25000
  }
}
```

Ticket 有效期 60 秒，只能使用一次。每次重连都要重新申请。

### 4.2 建立连接

```text
wss://<设备IP>:8099/openapi/v1/ws?ticket=<ticket>
```

连接成功后服务端发送：

```json
{
  "type": "ready",
  "protocol": "aibox.openapi.v1",
  "connection_id": "wss_xxx",
  "server_time_ms": 1785600000000,
  "max_message_bytes": 4194304,
  "heartbeat_interval_ms": 25000
}
```

### 4.3 发送请求

```json
{
  "type": "request",
  "id": "ws-call-001",
  "uri": "/openapi/v1/tasks/list",
  "request_id": "req-task-list-001",
  "param": {"page_size": 50}
}
```

响应：

```json
{
  "type": "response",
  "id": "ws-call-001",
  "uri": "/openapi/v1/tasks/list",
  "code": 0,
  "msg": "OK",
  "request_id": "req-task-list-001",
  "server_time_ms": 1785600000000,
  "result": {}
}
```

`id` 用于关联同一 WSS 上的并发请求。WSS 消息内不需要、也不能通过 `token` 字段切换身份。

保活：

```json
{"type":"ping"}
```

服务端返回：

```json
{"type":"pong","server_time_ms":1785600000000}
```

## 5. 任务对接流程

### 5.1 获取可用节点

先调用：

```text
/openapi/v1/nodes/catalog
/openapi/v1/nodes/schema
```

查询单个节点配置：

```json
{
  "uri": "/openapi/v1/nodes/schema",
  "param": {"node_type": "netclient"}
}
```

响应中的 `schema` 是该设备当前版本真实支持的节点配置。客户端不得硬编码插件数量或根据硬件型号猜测节点参数。

### 5.2 创建任务

```json
{
  "uri": "/openapi/v1/tasks/create",
  "request_id": "req-task-create-001",
  "idempotency_key": "task-create-task-001",
  "param": {
    "task": {
      "task_id": "task-001",
      "task_name": "入口检测",
      "schema_version": 1,
      "runtime_location": "local",
      "nodes": [
        {
          "node_id": "input-1",
          "node_type": "netclient",
          "category": "custom-input",
          "node_schema_version": 1,
          "config": {"url": "rtsp://192.168.1.20/live"}
        }
      ],
      "edges": []
    }
  }
}
```

成功响应返回完整任务，其中 `revision=1`：

```json
{
  "code": 0,
  "msg": "OK",
  "result": {
    "task_id": "task-001",
    "task_name": "入口检测",
    "revision": 1,
    "runtime_location": "local",
    "desired_state": "stopped",
    "nodes": [
      {
        "node_id": "input-1",
        "node_type": "netclient",
        "category": "custom-input",
        "node_schema_version": 1,
        "config": {"url": "rtsp://192.168.1.20/live"}
      }
    ],
    "edges": []
  }
}
```

### 5.3 修改任务

先调用 `tasks/get` 获得最新任务和 `revision`，修改完整任务后提交：

```json
{
  "uri": "/openapi/v1/tasks/save",
  "request_id": "req-task-save-001",
  "idempotency_key": "task-save-task-001-r1",
  "param": {
    "expected_revision": 1,
    "task": {
      "task_id": "task-001",
      "task_name": "入口检测-更新",
      "schema_version": 1,
      "runtime_location": "local",
      "nodes": [],
      "edges": []
    }
  }
}
```

`tasks/save` 是完整替换，不是局部 Patch。版本冲突返回：

```json
{
  "code": 40901,
  "msg": "TASK_REVISION_CONFLICT",
  "result": {
    "error": {
      "expected_revision": 1,
      "current_revision": 2,
      "retryable": false
    }
  }
}
```

发生冲突后重新读取任务，由客户决定如何合并，不能直接覆盖。

### 5.4 启动任务

```json
{
  "uri": "/openapi/v1/tasks/start",
  "request_id": "req-task-start-001",
  "idempotency_key": "task-start-task-001-r2",
  "param": {"task_id": "task-001", "revision": 2}
}
```

异步受理响应为 HTTP `202`：

```json
{
  "code": 0,
  "msg": "ACCEPTED",
  "result": {
    "operation_id": "op_xxx",
    "state": "queued",
    "progress": 0
  }
}
```

轮询 Operation：

```json
{
  "uri": "/openapi/v1/operations/get",
  "param": {"operation_id": "op_xxx"}
}
```

只有 `state=succeeded` 才表示执行成功。终态包括：

```text
succeeded | failed | canceled | interrupted
```

最后调用 `/openapi/v1/tasks/runtime` 核对实际运行状态。

### 5.5 停止与删除任务

停止：

```json
{
  "uri": "/openapi/v1/tasks/stop",
  "idempotency_key": "task-stop-task-001",
  "param": {"task_id": "task-001"}
}
```

删除：

```json
{
  "uri": "/openapi/v1/tasks/delete",
  "idempotency_key": "task-delete-task-001-r2",
  "param": {"task_id": "task-001", "expected_revision": 2}
}
```

## 6. 实时事件

当前支持：

| Topic | 所需 Scope | `resource.type` | 用途 |
|---|---|---|---|
| `alarm.created` | `alarm.read` | `alarm` | 新告警产生 |
| `alarm.deleted` | `alarm.read` | `alarm` | 告警被删除 |
| `record.created` | `media.read` | `record` | 新录像产生 |
| `record.deleted` | `media.read` | `record` | 录像被删除 |
| `task.created` | `task.read` | `task` | 任务定义已创建 |
| `task.updated` | `task.read` | `task` | 任务定义已修改 |
| `task.deleted` | `task.read` | `task` | 任务定义已删除 |
| `task.state.changed` | `task.read` | `task` | 任务运行状态变化 |
| `node.state.changed` | `task.read` | `node` | 节点运行状态可能变化 |
| `plugin.catalog.changed` | `task.read` | `plugin_catalog` | 插件安装、更新、启停或卸载后目录变化 |
| `device.state.changed` | `device.read` | `device` | 设备云控制通道连接状态变化 |
| `face.library.changed` | `face.read` | `face_library` | 人脸或人脸分组发生变更 |
| `upgrade.catalog.changed` | `upgrade.manage` | `upgrade_catalog` | 在线升级候选检查结果变化 |
| `upgrade.state.changed` | `upgrade.manage` | `upgrade` | 升级阶段或终态变化 |
| `retrieval.settings.changed` | `retrieval.use` | `retrieval_index` | 智能检索设置已保存或正在应用 |
| `retrieval.sync.queued` | `retrieval.use` | `retrieval_index` | 手动索引同步请求已进入有界队列 |
| `retrieval.index.changed` | `retrieval.use` | `retrieval_index` | 索引重建、同步或设置应用已完成 |
| `operation.state.changed` | 已认证 | `operation` | 当前认证主体提交的异步操作状态变化 |

事件按认证主体和 Scope 过滤。`operation.state.changed` 是私有事件：普通客户端、其他客户端以及通配管理账号的订阅都不能收到不属于自己的 Operation 事件。查询 Operation 时仍以第 8.10 节的权限规则为准。

任务定义、节点和插件事件采用“轻量事件 + 快照读取”模型，避免在 WSS 中重复发送大型配置：

| Topic | 收到事件后的权威读取接口 |
|---|---|
| `task.created`、`task.updated` | `tasks/get` |
| `task.deleted` | 从客户侧缓存删除对应 `resource.id` |
| `task.state.changed` | `tasks/runtime` |
| `node.state.changed` | `tasks/node_metrics` |
| `plugin.catalog.changed` | `nodes/catalog` 或 `plugins/list` |
| `device.state.changed` | `device/get`、`device/metrics` |
| `face.library.changed` | `faces/folders`、`faces/list` |
| `upgrade.catalog.changed` | `upgrades/check` |
| `upgrade.state.changed` | `upgrades/status` |
| `retrieval.settings.changed` | `retrieval/settings/get` |
| `retrieval.sync.queued`、`retrieval.index.changed` | `retrieval/index/status` |

`device.state.changed` 当前表示盒子到云控制面的归一化连接状态 `offline/connecting/online`，不等同于 CPU、NPU、内存等硬件指标变化；硬件指标按需调用 `device/metrics`。

人脸事件只发送分组标识、影响数量和快照地址，不发送姓名、电话号码、证件号、图片或特征向量。升级事件不发送下载 URL、本地路径或校验密钥。检索事件不发送查询文本、内部数据库/模型路径或命中结果；检索结果只在发起 `retrieval/query` 的同步响应中返回。

订阅：

```json
{
  "uri": "/openapi/v1/events/subscribe",
  "param": {"topics": ["task.state.changed", "operation.state.changed", "alarm.created"]}
}
```

响应：

```json
{
  "code": 0,
  "msg": "OK",
  "result": {
    "subscription_id": "sub_xxx",
    "topics": ["task.state.changed", "operation.state.changed", "alarm.created"],
    "delivery": "realtime+poll",
    "replay_items": [],
    "next_cursor": "evc_xxx:100",
    "expires_in_ms": 1800000
  }
}
```

通过 WSS 订阅时 `delivery` 为 `realtime+poll`；通过 HTTPS 订阅时为 `poll`。

WSS 事件：

```json
{
  "type": "event",
  "subscription_id": "sub_xxx",
  "event_id": "evt_xxx_101",
  "sequence": 101,
  "cursor": "evc_xxx:101",
  "topic": "record.created",
  "occurred_at_ms": 1785600000000,
  "resource": {"type": "record", "id": "rec_xxx"},
  "data": {"task_id": "task-001", "filename": "record.mp4"}
}
```

事件是至少一次交付。客户端必须：

1. 按 `event_id` 去重。
2. 业务落库后调用 `events/ack`。
3. 保存最后成功处理的 `cursor`。
4. WSS 断线后重新订阅，并用 `events/poll` 补偿遗漏事件。

客户端不得把 WSS 在线等同于状态永远最新。首次连接、重连、游标过期 `40904` 或事件声明 `snapshot_required=true` 时，必须调用上表的权威读取接口重建本地快照。

## 7. 媒体、录像和报警

### 7.1 获取任务输出地址

```json
{
  "uri": "/openapi/v1/media/outputs",
  "param": {"task_id": "task-001"}
}
```

响应：

```json
{
  "code": 0,
  "msg": "OK",
  "result": {
    "task_id": "task-001",
    "items": [
      {
        "output_id": "output-1",
        "node_id": "output-1",
        "protocol": "rtsp",
        "enabled": true,
        "codec": "h264",
        "port": 8554,
        "url": "rtsp://device.example/stream",
        "urls": ["rtsp://device.example/stream"],
        "credentials_included": false,
        "updated_at_ms": 1785600000000
      }
    ]
  }
}
```

### 7.2 下载录像

先查询录像：

```json
{
  "uri": "/openapi/v1/records/list",
  "param": {"task_id": "task-001", "page_size": 20}
}
```

再申请下载地址：

```json
{
  "uri": "/openapi/v1/records/download_ticket",
  "param": {"record_id": "rec_xxx"}
}
```

响应：

```json
{
  "code": 0,
  "msg": "OK",
  "result": {
    "record_id": "rec_xxx",
    "url": "/record/download?ticket=v1.xxx",
    "expires_at_ms": 1785600300000,
    "range_supported": true
  }
}
```

使用同一设备 HTTPS 地址下载：

```bash
curl -L -H "Range: bytes=0-1048575" \
  "https://<设备IP>:8099/record/download?ticket=v1.xxx" \
  -o record.part
```

Ticket 最长有效 5 分钟。过期后重新申请，不得长期缓存。

## 8. API 参考

当前实现共开放 `64` 个 URI（其中任务启动、停止由同一注册器动态生成）。8.1～8.7 提供快速索引，8.8～8.17 给出数据模型和逐接口说明。

表中 `param` 是请求参数，`result` 是成功响应的主要字段。`?` 表示可选。

逐接口卡片中形如 `POST /openapi/v1/...` 的“请求地址”是 Command URI 简写。HTTPS 的真实请求路径始终是 `/openapi/v1/command`；WSS 请求把同一 URI 放入 `type=request` 消息。

### 8.1 会话、Operation 和事件

| URI | Scope | param | result |
|---|---|---|---|
| `/openapi/v1/auth/token` | 无 | `client_id`、`client_secret` | `access_token`、`expires_in`、`scopes` |
| `/openapi/v1/session/get` | 已认证 | `{}` | `subject`、`client_id`、`scopes`、`expires_at` |
| `/openapi/v1/ws/ticket` | 已认证 | `{}` | `ticket`、`expires_at_ms`、`websocket_path` |
| `/openapi/v1/operations/get` | 已认证 | `operation_id` | Operation |
| `/openapi/v1/operations/list` | 已认证 | `page_size?` | `items`、`count` |
| `/openapi/v1/operations/cancel` | 已认证 | `operation_id` | Operation |
| `/openapi/v1/events/subscribe` | 按 Topic | `topics[]`、`cursor?`、`subscription_id?` | `subscription_id`、`replay_items`、`next_cursor` |
| `/openapi/v1/events/poll` | 按 Topic | `subscription_id`、`cursor?`、`page_size?` | `items`、`next_cursor`、`has_more` |
| `/openapi/v1/events/ack` | 按 Topic | `subscription_id`、`cursor` | `acknowledged_cursor` |
| `/openapi/v1/events/unsubscribe` | 按 Topic | `subscription_id` | `deleted` |

### 8.2 设备、节点和插件

| URI | Scope | param | result |
|---|---|---|---|
| `/openapi/v1/device/get` | `device.read` | `{}` | 设备 ID、版本和授权状态 |
| `/openapi/v1/device/metrics` | `device.read` | `{}` | `compute[]`、`storage[]`、`collected_at_ms` |
| `/openapi/v1/device/runtime_locations` | `device.read` | `{}` | 可用运行位置 |
| `/openapi/v1/device/capabilities` | `device.read` | `{}` | `runtime_locations`、`node_types`、`features` |
| `/openapi/v1/device/network/get` | `device.read` | `{}` | 网络接口快照 |
| `/openapi/v1/nodes/catalog` | `task.read` | `{}` | 组件分组和加载进度 |
| `/openapi/v1/nodes/schema` | `task.read` | `node_type` | 节点 Schema |
| `/openapi/v1/graphs/validate` | `task.write` | `task` | `valid`、`warnings` |
| `/openapi/v1/plugins/list` | `task.read` | `{}` | 插件列表 |
| `/openapi/v1/plugins/health` | `task.read` | `{}` | 插件健康状态 |

### 8.3 任务

| URI | Scope | param | result |
|---|---|---|---|
| `/openapi/v1/tasks/list` | `task.read` | `cursor?`、`page_size?`、`name?` | `items`、`total`、`next_cursor` |
| `/openapi/v1/tasks/get` | `task.read` | `task_id` | Task |
| `/openapi/v1/tasks/create` | `task.write` | `task` | Task |
| `/openapi/v1/tasks/save` | `task.write` | `task`、`expected_revision` | Task |
| `/openapi/v1/tasks/clone` | `task.write` | `source_task_id`、`new_task_id`、`new_task_name?` | Task |
| `/openapi/v1/tasks/delete` | `task.write` | `task_id`、`expected_revision` | `deleted` |
| `/openapi/v1/tasks/start` | `task.execute` | `task_id`、`revision` | Operation，HTTP 202 |
| `/openapi/v1/tasks/stop` | `task.execute` | `task_id` | Operation，HTTP 202 |
| `/openapi/v1/tasks/restart` | `task.execute` | `task_id`、`revision` | Operation，HTTP 202 |
| `/openapi/v1/tasks/apply_and_start` | `task.write task.execute` | `task`、`expected_revision` | Operation，HTTP 202 |
| `/openapi/v1/tasks/runtime` | `task.read` | `task_id` | 运行状态 |
| `/openapi/v1/tasks/node_metrics` | `task.read` | `task_id` | `nodes[]`、`collected_at_ms` |

### 8.4 媒体、录像、报警和日志

| URI | Scope | param | result |
|---|---|---|---|
| `/openapi/v1/media/outputs` | `media.read` | `task_id` | `items[]` |
| `/openapi/v1/records/list` | `media.read` | `cursor?`、`page_size?`、`task_id?`、`year?`、`month?`、`day?` | `items`、`total`、`next_cursor` |
| `/openapi/v1/records/get` | `media.read` | `record_id` | Record |
| `/openapi/v1/records/download_ticket` | `media.read` | `record_id` | `url`、`expires_at_ms`、`range_supported` |
| `/openapi/v1/records/delete` | `media.manage` | `record_id` | `deleted` |
| `/openapi/v1/alarms/list` | `alarm.read` | `cursor?`、`page_size?`、`task_name?` | `items`、`total`、`next_cursor` |
| `/openapi/v1/alarms/get` | `alarm.read` | `alarm_id` | Alarm |
| `/openapi/v1/alarms/delete` | `alarm.manage` | `alarm_id` | `deleted` |
| `/openapi/v1/logs/list` | `log.read` | `cursor?`、`page_size?`、`source?`、`level?`、`keyword?`、`start_time?`、`end_time?` | `items`、`total`、`next_cursor` |
| `/openapi/v1/logs/get` | `log.read` | `log_id` | Log |

### 8.5 人脸

| URI | Scope | param | result |
|---|---|---|---|
| `/openapi/v1/faces/folders` | `face.read` | `{}` | 文件夹列表 |
| `/openapi/v1/faces/list` | `face.read` | `cursor?`、`page_size?` | `items`、`total`、`next_cursor` |
| `/openapi/v1/faces/get` | `face.read` | `face_id` | Face |
| `/openapi/v1/faces/register` | `face.write` | Face | Operation，HTTP 202 |
| `/openapi/v1/faces/update` | `face.write` | Face | Operation，HTTP 202 |
| `/openapi/v1/faces/delete` | `face.write` | `face_id` | `deleted` |
| `/openapi/v1/faces/batch_import` | `face.write` | `items[1..8]` | Operation，HTTP 202 |

Face 请求对象：

```json
{
  "face_id": "employee-001",
  "name": "示例人员",
  "work_id": "E001",
  "department": "研发部",
  "phone": "",
  "gender": 0,
  "age": 30,
  "category": 1,
  "photo_base64": "<不带 data:image 前缀的 Base64>"
}
```

### 8.6 智能检索和升级

| URI | Scope | param | result |
|---|---|---|---|
| `/openapi/v1/retrieval/status` | `retrieval.use` | `{}` | 服务状态 |
| `/openapi/v1/retrieval/index/status` | `retrieval.use` | `{}` | 索引状态 |
| `/openapi/v1/retrieval/settings/get` | `retrieval.use` | `{}` | 检索设置 |
| `/openapi/v1/retrieval/settings/set` | `retrieval.use` | 非空设置对象 | 更新结果 |
| `/openapi/v1/retrieval/sync` | `retrieval.use` | 可选同步参数 | 同步结果 |
| `/openapi/v1/retrieval/query` | `retrieval.use` | `query`、`top_k?`、`media_scope?` | 检索结果 |
| `/openapi/v1/upgrades/check` | `upgrade.manage` | `{}` | 可用版本 |
| `/openapi/v1/upgrades/status` | `upgrade.manage` | `{}` | 升级状态 |
| `/openapi/v1/upgrades/apply` | `upgrade.manage` | `kind`、`release_id?` | 启动结果 |
| `/openapi/v1/upgrades/cancel` | `upgrade.manage` | `{}` | 取消结果 |

`retrieval/query` 中 `top_k` 为 1～200，`media_scope` 为 `all`、`image` 或 `video`。升级 `kind` 为 `app` 或 `firmware`。

### 8.7 客户端密钥管理（交付方使用）

第三方客户通常不调用本组接口。交付方使用具有 `openapi.clients.manage` Scope 的管理 Token 创建、轮换或禁用客户凭证。

`openapi.clients.manage` 和通配符 `*` 是本地交付管理员的保留权限，不能写入第三方客户端的 `scopes[]`。服务端同时禁止任何 `token_use=openapi_client` 的 Token 调用本组接口；即使旧版本数据库中存在误配 Scope，也不会放行。

| URI | param | result |
|---|---|---|
| `/openapi/v1/clients/create` | `display_name`、`scopes[]` | Client 和只返回一次的 `client_secret` |
| `/openapi/v1/clients/list` | `{}` | `items`、`total`，不返回密钥 |
| `/openapi/v1/clients/get` | `client_id` | Client，不返回密钥 |
| `/openapi/v1/clients/update` | `client_id`、`display_name?`、`scopes?`、`enabled?` | Client |
| `/openapi/v1/clients/rotate_secret` | `client_id` | 新 `client_secret`，旧密钥和旧 Token 立即失效 |

创建示例：

```json
{
  "uri": "/openapi/v1/clients/create",
  "request_id": "req-client-create-001",
  "param": {
    "display_name": "客户业务平台",
    "scopes": ["device.read", "task.read", "task.write", "task.execute"]
  }
}
```

创建和轮换响应中的 `client_secret` 只返回一次。如果响应丢失，应执行轮换，不能从设备查询原密钥。

### 8.8 公共数据模型

以下模型被多个接口复用。除特别注明外，时间戳字段均为 Unix 毫秒；客户端必须忽略未知新增字段。

#### 8.8.1 Task

| 字段 | 类型 | 必须 | 说明 |
|---|---|---|---|
| `task_id` | string | 是 | 1～128 字节；首字符为字母或数字，其余允许字母、数字、`.`、`_`、`:`、`-` |
| `task_name` | string | 是 | 1～256 字节，不允许控制字符 |
| `revision` | int64 | 写入否 | 服务端版本号；创建后为 1；保存、删除、启动时用于并发控制 |
| `schema_version` | int | 否 | 默认 1 |
| `runtime_location` | string | 否 | 默认 `local`；取值必须来自 `device/runtime_locations` |
| `desired_state` | string | 否 | `running` 或 `stopped` |
| `nodes` | array | 是 | 最多 256 个 Node |
| `edges` | array | 是 | 最多 1024 条 Edge；图必须无环 |
| `created_at_ms` | int64 | 响应 | 创建时间 |
| `updated_at_ms` | int64 | 响应 | 更新时间 |

单个 Task JSON 上限 2 MiB；单个节点 `config` 上限 1 MiB。

Node：

| 字段 | 类型 | 必须 | 说明 |
|---|---|---|---|
| `node_id` | string | 是 | 任务内唯一，格式同 `task_id` |
| `node_type` | string | 是 | 来自 `nodes/catalog`，不得写死插件清单 |
| `category` | string | 否 | `input`、`algorithm`、`process`、`output` 或插件返回的分类 |
| `node_schema_version` | int | 否 | 默认 1 |
| `implementation_version` | string | 否 | 节点实现版本 |
| `config` | object | 是 | 按 `nodes/schema` 返回的 Schema 填写 |

Edge：

| 字段 | 类型 | 必须 | 说明 |
|---|---|---|---|
| `from` | string | 是 | 上游 `node_id` |
| `from_port` | string | 否 | 默认 `default` |
| `to` | string | 是 | 下游 `node_id`；不可等于 `from` |
| `to_port` | string | 否 | 默认 `default` |

#### 8.8.2 TaskRuntime

| 字段 | 类型 | 说明 |
|---|---|---|
| `task_id` | string | 任务编号 |
| `state` | string | `starting`、`running`、`stopping`、`stopped`、`error` |
| `phase` | string | 正常运行时为 `healthy`，其余通常与 `state` 相同 |
| `desired_state` | string | 期望状态：`running` 或 `stopped` |
| `actual_running` | bool | 设备运行时确认的实际运行状态 |
| `runtime_location` | string | 实际运行位置 |
| `started_at_ms` | int64 | 最近启动时间；未启动时可能为 0 |
| `stopped_at_ms` | int64 | 最近停止时间；未停止时可能为 0 |
| `updated_at_ms` | int64 | 状态更新时间 |
| `last_error` | string/null | 最近运行错误；无错误为 `null` |

状态判断必须以 `actual_running` 和 `state` 为准，不能只看任务定义中的 `desired_state`。

#### 8.8.3 NodeStatus

| 字段 | 类型 | 说明 |
|---|---|---|
| `node_id` | string | 节点编号 |
| `node_type` | string | 节点类型 |
| `state` | string | 当前为任务运行态的聚合值 |
| `metrics_available` | bool | 当前版本固定为 `false`，表示尚无插件级 CPU/FPS/延迟指标 |
| `source` | string | 当前为 `task_runtime_aggregate` |

`tasks/node_metrics` 当前提供“任务运行态映射到每个节点”的可靠状态快照，不代表插件进程级健康检查。客户端不得把 `metrics_available=false` 当作节点故障。

#### 8.8.4 Operation

| 字段 | 类型 | 说明 |
|---|---|---|
| `operation_id` | string | 异步操作编号 |
| `kind` | string | 如 `task.start`、`face.register` |
| `resource` | object | `{type,id}` |
| `request_id` | string | 原请求追踪 ID |
| `state` | string | `queued`、`running`、`succeeded`、`failed`、`canceled`、`interrupted` |
| `progress` | int | 0～100 |
| `code` / `msg` | int/string | 操作最终业务码和消息 |
| `created_at_ms` | int64 | 创建时间 |
| `started_at_ms` | int64 | 开始时间 |
| `finished_at_ms` | int64 | 完成时间 |
| `updated_at_ms` | int64 | 更新时间 |
| `cancelable` | bool | 只有仍在 `queued` 的操作可取消 |
| `result` | any | 成功结果，仅成功完成后出现 |
| `error` | object | 失败详情，仅失败时出现 |

#### 8.8.5 Device、Record、Alarm、Log、Face

Device 基本信息：

| 字段 | 类型 | 说明 |
|---|---|---|
| `device_id` | string | 设备编号 |
| `guid` | string | 设备唯一标识 |
| `vendor` | string | 厂商 |
| `application_version` | string | AIBox 应用版本 |
| `firmware_version` | string | 固件版本 |
| `authorization` | object/string | 当前授权状态 |
| `device_time` | string/int64 | 设备时间 |

Record：`record_id`、`task_id`、`task_name`、`filename`、`size_bytes`、`duration_ms`、`modified_at_ms`、`begin_at_ms`、`codec`、`width`、`height`、`mode`、`download_available`；可下载时另有 `download_url`、`download_expires_at_ms`。

Alarm：`alarm_id`、`task_id`、`task_name`、`alarm_type`、`summary`、`occurred_at`、`image_available`。

Log：`log_id`、`device_id`、`source`、`level`、`message`、`occurred_at`。服务端会对 Token、密码、密钥等敏感片段脱敏，并将超长消息截断至约 8 KiB。

Face 稳定字段：`face_id`、`name`、`gender`、`age`、`work_id`、`id_card_no`、`ic_card_no`、`department`、`phone`、`category`、`photo_available`、`create_time`、`update_time`。不同设备版本可能增加只读字段。

### 8.9 鉴权、会话、WSS 和客户端密钥 API

#### 8.9.1 获取访问令牌

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/auth/token` |
| 功能说明 | 用独立第三方密钥换取短期 Token；仅允许 HTTPS。 |
| 所需权限 | 无 |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`client_id`（必填，最长 128）、`client_secret`（必填，最长 256）。

**成功响应**

`access_token`、`token_type`、`token_use`、`expires_in`、`expires_at`、`client_id`、`scopes[]`。

**补充说明**

- 注意：认证失败可能触发 429 限流；按 `retry_after_ms` 等待。

#### 8.9.2 查询当前会话

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/session/get` |
| 功能说明 | 确认 Token 身份、权限和过期时间。 |
| 所需权限 | 已认证 |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

`subject`、`client_id?`、`scopes[]`、`expires_at`、`token_use`。

#### 8.9.3 签发 WebSocket 连接票据

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/ws/ticket` |
| 功能说明 | 签发一次性 WSS Ticket；仅允许 HTTPS 调用。 |
| 所需权限 | 已认证 |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

`ticket`、`expires_at_ms`、`websocket_path`、`protocol`、`max_message_bytes`、`heartbeat_interval_ms`。

**补充说明**

- 连接地址：`wss://<设备IP>:8099<websocket_path>?ticket=<ticket>`；Ticket 不得复用或记录到普通日志。

以下 5 个接口仅供设备本地交付管理员使用，均要求 HTTPS、`openapi.clients.manage`。任何 `token_use=openapi_client` 的 Token 都不能调用本组接口；云端租户管理员应通过 Device Gateway 的独立管理面完成集中签发和吊销。


#### 8.9.4 创建第三方客户端

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/clients/create` |
| 功能说明 | 创建第三方客户端。 |
| 所需权限 | `openapi.clients.manage` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`display_name`（必填，1～128 字节）、`scopes[]`（必填，1～64 项）。

**成功响应**

Client 字段以及 `client_secret`、`secret_returned_once=true`。

**补充说明**

- 注意：`client_secret` 只返回一次。

#### 8.9.5 查询第三方客户端列表

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/clients/list` |
| 功能说明 | 列出全部第三方客户端，不返回密钥。 |
| 所需权限 | `openapi.clients.manage` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

`items[]`、`total`；每项含 `client_id`、`display_name`、`scopes`、`enabled`、`created_at_ms`、`updated_at_ms`、`last_used_at_ms`、`last_rotated_at_ms`。

#### 8.9.6 查询第三方客户端

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/clients/get` |
| 功能说明 | 查询一个第三方客户端，不返回密钥。 |
| 所需权限 | `openapi.clients.manage` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`client_id`（必填）。

**成功响应**

Client。

#### 8.9.7 更新第三方客户端

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/clients/update` |
| 功能说明 | 修改名称、Scope 或启停客户端。 |
| 所需权限 | `openapi.clients.manage` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`client_id` 必填；`display_name`、`scopes[]`、`enabled` 至少按需提供一项。

**成功响应**

更新后的 Client。

**补充说明**

- 注意：修改 Scope 或启用状态会使该客户端现有 Token 失效。

#### 8.9.8 轮换客户端密钥

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/clients/rotate_secret` |
| 功能说明 | 轮换客户端密钥。 |
| 所需权限 | `openapi.clients.manage` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`client_id`（必填）。

**成功响应**

`client_id`、`client_secret`、`secret_returned_once=true`、`rotated_at_ms`。

**补充说明**

- 注意：旧密钥及该客户端已有 Token 立即失效。

### 8.10 Operation API

异步写接口返回 HTTP 202 和 Operation。Operation 只对提交它的认证主体可见；具有通配管理权限的管理员除外。

#### 8.10.1 查询异步操作

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/operations/get` |
| 功能说明 | 查询单个异步操作。 |
| 所需权限 | 已认证，无额外 Scope |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`operation_id`（必填）。

**成功响应**

Operation。

#### 8.10.2 查询异步操作列表

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/operations/list` |
| 功能说明 | 查询当前认证主体最近的异步操作。 |
| 所需权限 | 已认证，无额外 Scope |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`page_size` 可选，1～200，默认 50。

**成功响应**

`items[]`（Operation）、`count`。

#### 8.10.3 取消异步操作

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/operations/cancel` |
| 功能说明 | 取消尚未开始的异步操作。 |
| 所需权限 | 已认证，无额外 Scope |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`operation_id`（必填）。

**成功响应**

取消后的 Operation。

**补充说明**

- 注意：仅 `queued` 可取消；其他状态返回 `40903`。

### 8.11 事件 API

| Topic | 所需权限 | `data` 关键字段 |
|---|---|---|
| `alarm.created`、`alarm.deleted` | `alarm.read` | 告警安全摘要；详情调用 `alarms/get` |
| `record.created`、`record.deleted` | `media.read` | `task_id`、安全文件名等；详情调用 `records/get` |
| `task.created`、`task.updated` | `task.read` | `task_id`、`task_name`、`revision`、`desired_state`、`updated_at_ms`、`snapshot_required` |
| `task.deleted` | `task.read` | `task_id`、`deleted=true` |
| `task.state.changed` | `task.read` | 任务运行时摘要、`reason`；权威状态调用 `tasks/runtime` |
| `node.state.changed` | `task.read` | `task_id`、`reason`、`snapshot_required`、`snapshot_uri` |
| `plugin.catalog.changed` | `task.read` | `reason`、目录 `revision`、`ready/discovered/settled`、`snapshot_uri` |
| `device.state.changed` | `device.read` | `state`、`control_plane_state`、`detail` |
| `face.library.changed` | `face.read` | `reason`、`folder_id?`、`affected_count`、快照 URI |
| `upgrade.catalog.changed` | `upgrade.manage` | 检查状态及应用/固件候选的脱敏版本摘要 |
| `upgrade.state.changed` | `upgrade.manage` | `kind`、`release_id`、`phase`、`progress`、版本、时间和快照 URI |
| `retrieval.settings.changed` | `retrieval.use` | 保存/应用状态、索引计数和快照 URI |
| `retrieval.sync.queued` | `retrieval.use` | `sync_queued`、当前索引计数和快照 URI |
| `retrieval.index.changed` | `retrieval.use` | 同步/重建结果、索引计数、阶段、时间和快照 URI |
| `operation.state.changed` | 当前 Operation 的提交主体 | 完整公开 Operation 快照 |

一次订阅 1～24 个 Topic；同一认证主体最多 8 个有效订阅；订阅空闲 30 分钟过期。订阅任一 Topic 缺少 Scope 时，整个订阅请求返回 HTTP 403，不创建部分订阅。

事件对象字段：`type,event_id,sequence,cursor,topic,occurred_at_ms,resource{type,id},data`。WSS 推送时另含 `subscription_id`。

事件日志是进程内有界日志，不是永久消息队列。交付语义为至少一次，同一 `event_id` 可能通过实时推送和断线补偿重复到达；客户端必须去重。服务重启或游标早于当前日志窗口时返回 `40904 EVENT_CURSOR_EXPIRED`，客户端应读取业务快照后用新游标重新订阅。

#### 8.11.1 订阅事件

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/events/subscribe` |
| 功能说明 | 建立或恢复订阅。 |
| 所需权限 | 由订阅 Topic 决定 |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`topics[]` 必填，数组长度 1～24；服务端会对重复 Topic 去重，客户端仍应避免重复发送。`cursor`、`subscription_id` 可选。

**成功响应**

`subscription_id`、`topics[]`、`delivery`、`replay_items[]`、`next_cursor`、`expires_in_ms`。

**补充说明**

- `delivery`：HTTPS 为 `poll`；WSS 为 `realtime+poll`。
- `operation.state.changed` 只按创建 Operation 时固定的认证主体投递，不能通过订阅通配 Topic 或管理员身份旁路读取。

#### 8.11.2 拉取订阅事件

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/events/poll` |
| 功能说明 | 分页拉取事件，作为 HTTP 主通道或 WSS 断线补偿。 |
| 所需权限 | 由订阅 Topic 决定 |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`subscription_id` 必填；`cursor` 可选；`page_size` 1～100，默认 100。

**成功响应**

`subscription_id`、`items[]`、`next_cursor`、`has_more`。

#### 8.11.3 确认事件游标

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/events/ack` |
| 功能说明 | 确认已完成业务处理的事件位置。 |
| 所需权限 | 由订阅 Topic 决定 |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`subscription_id`、`cursor` 必填。

**成功响应**

`subscription_id`、`acknowledged_cursor`。

#### 8.11.4 取消事件订阅

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/events/unsubscribe` |
| 功能说明 | 释放订阅。 |
| 所需权限 | 由订阅 Topic 决定 |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`subscription_id` 必填。

**成功响应**

`subscription_id`、`deleted=true`。

事件为至少一次交付；必须按 `event_id` 去重。`40904` 表示游标不再可恢复，客户端应先重新拉取资源快照，再从新游标订阅。


### 8.12 设备、节点目录和插件 API

#### 8.12.1 查询设备基本信息

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/device/get` |
| 功能说明 | 读取设备身份、软件/固件版本、授权和设备时间。 |
| 所需权限 | `device.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

Device，字段见 8.8.5。

#### 8.12.2 查询设备运行指标

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/device/metrics` |
| 功能说明 | 读取设备算力、内存、温度和存储快照。 |
| 所需权限 | `device.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

`collected_at_ms`、`compute[]`、`storage[]`。

`compute[]` 每项：

| 字段 | 类型 | 说明 |
|---|---|---|
| `name` | string | 本机或算力卡名称 |
| `cpu_percent` / `npu_percent` | number | CPU/NPU 使用率百分比 |
| `memory_percent` | number | 内存使用率 |
| `memory_total_bytes` / `memory_used_bytes` | int64 | 内存容量/已用字节 |
| `cmm_percent` | number | CMM 使用率 |
| `cmm_total_bytes` / `cmm_used_bytes` | int64 | CMM 容量/已用字节 |
| `temperature_celsius` | number | 摄氏温度 |

`storage[]` 每项：`storage_id`、`total_bytes`、`used_bytes`。百分比字段可能短暂为 0，应结合连续采样判断，不要用单点值判定故障。


#### 8.12.3 查询可用运行位置

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/device/runtime_locations` |
| 功能说明 | 查询任务可部署位置。 |
| 所需权限 | `device.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

`items[]`，每项 `label`、`value`、`device_id`、`kind`；`kind` 为 `local` 或 `compute_card`。

**补充说明**

- 注意：创建任务时保存 `value` 到 `runtime_location`。

#### 8.12.4 查询设备能力

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/device/capabilities` |
| 功能说明 | 能力协商；第三方页面初始化时应调用。 |
| 所需权限 | `device.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

`runtime_locations[]`、`node_types[]`、`features[]`。

**补充说明**

- 当前 Feature 可包含 `tasks`、`task_operations`、`alarms`、`plugin_inventory`、`upgrade_status`、`retrieval_status`。客户端只展示实际返回的能力。

#### 8.12.5 查询设备网络状态

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/device/network/get` |
| 功能说明 | 读取网络接口快照，不修改设备网络。 |
| 所需权限 | `device.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

`interfaces[]`；每项 `name`、`mode`、`ip`、`gateway`、`netmask`、`dns`，`mode` 为 `dhcp` 或 `static`。

#### 8.12.6 查询节点目录

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/nodes/catalog` |
| 功能说明 | 异步插件加载期间获取可用节点目录和加载进度。 |
| 所需权限 | `task.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

`groups[]`、`revision`、`ready`、`discovered`、`settled`。

**补充说明**

- `groups[]`：每组通常含分类信息和 `list[]` 组件；组件结构由插件提供。
- 正确加载方式：先立即渲染已有 `groups`；当 `ready=false` 或 `discovered!=settled` 时，按退避策略再次请求；当 `revision` 变化时用新快照整体替换旧目录。不得按“每页固定插件数量”截断，也不得把某次的 17/30 个组件写死。

#### 8.12.7 查询节点配置 Schema

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/nodes/schema` |
| 功能说明 | 取得指定节点的配置 Schema/表单元数据。 |
| 所需权限 | `task.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`node_type`（必填，来自目录）。

**成功响应**

`node_type`、`schema`。

**补充说明**

- `schema` 为插件定义的动态对象，通常包含名称、类别、默认值、字段列表、校验范围和版本。第三方应保留未知字段，按当前设备返回值生成表单；插件升级后重新获取。

#### 8.12.8 校验任务图

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/graphs/validate` |
| 功能说明 | 在保存或启动前校验任务图。 |
| 所需权限 | `task.write` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`task`（完整 Task）。

**成功响应**

`valid=true`、`warnings[]`。

**补充说明**

- 校验失败返回 `42201`，错误对象可能包含 `field`、`node_id`、`edge_index`、`detail`。服务端检查标识符、节点/边数量、引用、自环、重复边、环路和 JSON 大小。

#### 8.12.9 查询插件清单

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/plugins/list` |
| 功能说明 | 读取插件库存、版本、加载状态、授权/签名状态和任务引用。 |
| 所需权限 | `task.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

`currentPlatform`、`items[]`、`marketplaceReady`、`total`。

**补充说明**

- `items[]` 稳定字段包括：`id`、`type`、`name`、`label`、`labelText`、`version`、`description`、`vendor`、`developer`、`enabled`、`loaded`、`state`、`status`、`compatible`、`source`、`entry`、`packageName`、`packageSize`、`md5`、`buildTime`、`installedAt`、`updatedAt`、`loadTimeMs`、`errorCount`、`lastError`、`inUse`、`inUseByRunningTasks`、`referenceCount`、`runningReferenceCount`、`references[]`、`runningReferences[]`、`restartRequired`、`platform`、`currentPlatform`；商业授权、签名信息及新元数据可能按插件增加。

#### 8.12.10 查询插件健康状态

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/plugins/health` |
| 功能说明 | 获取适合监控展示的精简插件健康快照。 |
| 所需权限 | `task.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

`items[]`、`total`。

**补充说明**

- 每项：`type`、`version`、`enabled`、`state`、`loadTimeMs`、`errorCount`、`lastError`、`taskRefCount`；有待执行动作时含 `pendingAction{type,reason,scheduledAt}`。
- `state`：`loaded`、`disabled`、`error`、`installed`。

### 8.13 任务 API

#### 8.13.1 查询任务列表

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/tasks/list` |
| 功能说明 | 分页读取任务定义，按 `updated_at_ms` 倒序。 |
| 所需权限 | `task.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`cursor` 可选；`page_size` 1～200，默认 50；`name` 可选，按任务名称包含匹配。

**成功响应**

`items[]`（Task）、`total`、`has_more`、`next_cursor`。

#### 8.13.2 查询任务定义

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/tasks/get` |
| 功能说明 | 获取完整任务定义和当前 Revision。 |
| 所需权限 | `task.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`task_id` 必填。

**成功响应**

Task。

#### 8.13.3 创建任务

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/tasks/create` |
| 功能说明 | 创建新任务。 |
| 所需权限 | `task.write` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`task`（完整 Task，忽略传入的只读时间和 Revision）。

**成功响应**

创建后的 Task，`revision=1`。

**补充说明**

- 冲突：`task_id` 已存在返回资源状态冲突。

#### 8.13.4 保存任务

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/tasks/save` |
| 功能说明 | 全量替换已有任务定义。 |
| 所需权限 | `task.write` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`task`（完整 Task）、`expected_revision`（正整数）。

**成功响应**

保存后的 Task，Revision 增加。

**补充说明**

- 注意：先 `tasks/get`，用返回 Revision 保存；`40901` 时重新获取并处理冲突，不能盲目覆盖。

#### 8.13.5 复制任务

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/tasks/clone` |
| 功能说明 | 复制任务并保持副本为停止状态。 |
| 所需权限 | `task.write` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`source_task_id`、`new_task_id` 必填；`new_task_name` 可选。

**成功响应**

新 Task，`revision=1`、`desired_state=stopped`。

#### 8.13.6 删除任务

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/tasks/delete` |
| 功能说明 | 删除任务定义。 |
| 所需权限 | `task.write` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`task_id`、`expected_revision` 必填。

**成功响应**

`task_id`、`deleted=true`。

**补充说明**

- 建议：先停止任务并确认 `actual_running=false` 后删除。

#### 8.13.7 启动任务

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/tasks/start` |
| 功能说明 | 按指定 Revision 异步启动任务。 |
| 所需权限 | `task.execute` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 202 Accepted` |

**请求参数**

`task_id`、`revision` 必填。

**成功响应**

HTTP 202，`result` 为 Operation。
TaskRuntime。

#### 8.13.8 停止任务

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/tasks/stop` |
| 功能说明 | 异步停止任务。 |
| 所需权限 | `task.execute` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 202 Accepted` |

**请求参数**

`task_id` 必填。

**成功响应**

HTTP 202，`result` 为 Operation；Operation 成功结果为 TaskRuntime。

#### 8.13.9 重启任务

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/tasks/restart` |
| 功能说明 | 按指定 Revision 先停后启。 |
| 所需权限 | `task.execute` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 202 Accepted` |

**请求参数**

`task_id`、`revision` 必填。

**成功响应**

HTTP 202，`result` 为 Operation；成功结果为 TaskRuntime。

#### 8.13.10 保存并启动任务

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/tasks/apply_and_start` |
| 功能说明 | 原子业务流程“保存新定义并启动”，避免保存后漏启动。 |
| 所需权限 | 同时要求 `task.write` 和 `task.execute` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 202 Accepted` |

**请求参数**

`task`（完整 Task）、`expected_revision`。

**成功响应**

HTTP 202，`result` 为 Operation。
`task`（新定义）、`runtime`（TaskRuntime）。

#### 8.13.11 查询任务运行状态

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/tasks/runtime` |
| 功能说明 | 读取任务实际运行状态。 |
| 所需权限 | `task.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`task_id` 必填。

**成功响应**

TaskRuntime，字段见 8.8.2。

#### 8.13.12 查询任务节点状态

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/tasks/node_metrics` |
| 功能说明 | 一次取得任务状态及全部节点状态；适合任务编辑页/监控页定时刷新。 |
| 所需权限 | `task.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`task_id` 必填。

**成功响应**

`task_id`、`state`、`desired_state`、`actual_running`、`runtime_location`、`last_error`、`collected_at_ms`、`nodes[]`。

**补充说明**

- `nodes[]` 为 NodeStatus，字段见 8.8.3。

典型响应：

```json
{
  "code": 0,
  "msg": "OK",
  "result": {
    "task_id": "task-001",
    "state": "running",
    "desired_state": "running",
    "actual_running": true,
    "runtime_location": "ax.local",
    "last_error": null,
    "collected_at_ms": 1785600000000,
    "nodes": [
      {
        "node_id": "detector-1",
        "node_type": "intrusion",
        "state": "running",
        "metrics_available": false,
        "source": "task_runtime_aggregate"
      }
    ]
  }
}
```


#### 设备、任务、节点状态刷新建议

| 页面数据 | 初始化调用 | 建议刷新 |
|---|---|---|
| 设备版本/授权 | `device/get` | 进入页面及版本变化后 |
| CPU/NPU/内存/温度/存储 | `device/metrics` | 前台 3～5 秒；后台暂停或降频 |
| 任务定义 | `tasks/get` / `tasks/list` | 进入页面、保存完成及 Revision 冲突后 |
| 任务运行态 | `tasks/runtime` | 启停期间 500 ms～1 s，稳定后 3～5 s |
| 节点状态 | `tasks/node_metrics` | 与任务运行态同频，避免每个节点单独请求 |
| 插件加载 | `nodes/catalog` | 加载期退避轮询，`ready=true` 后停止 |

HTTP 轮询应加入 10%～20% 抖动；页面隐藏时降频。告警和录像新增优先使用 WSS 事件，不要高频扫列表。

### 8.14 媒体输出、录像、报警和日志 API

#### 8.14.1 查询任务媒体输出

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/media/outputs` |
| 功能说明 | 读取任务输出节点生成的 RTSP 地址。 |
| 所需权限 | `media.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`task_id` 必填。

**成功响应**

`task_id`、`items[]`。

**补充说明**

- 每项：`output_id`、`node_id`、`protocol`、`enabled`、`codec`、`port`、`url`、`urls[]`、`credentials_included`、`updated_at_ms`。
- 服务端会移除 URL 中的用户名/密码，`credentials_included=false`。地址暂不可用时 `items` 可为空，应结合任务运行态判断。

#### 8.14.2 查询录像列表

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/records/list` |
| 功能说明 | 分页查询录像。 |
| 所需权限 | `media.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`cursor` 可选；`page_size` 1～100，默认 50；`task_id`、`year`、`month`、`day` 可选。

**成功响应**

`items[]`（Record）、`total`、`total_bytes`、`next_cursor`。

#### 8.14.3 查询录像详情

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/records/get` |
| 功能说明 | 查询录像元数据。 |
| 所需权限 | `media.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`record_id` 必填，格式为 `rec_` 加 64 位十六进制摘要。

**成功响应**

Record。

#### 8.14.4 签发录像下载票据

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/records/download_ticket` |
| 功能说明 | 为一条录像签发短时下载地址。 |
| 所需权限 | `media.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`record_id` 必填。

**成功响应**

`record_id`、`url`、`expires_at_ms`、`range_supported=true`。

**补充说明**

- 使用：将相对 `url` 拼到当前设备 `https://<设备IP>:8099`；支持 HTTP Range。Ticket 过期后重新申请。

#### 8.14.5 删除录像

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/records/delete` |
| 功能说明 | 删除录像文件及对应记录。 |
| 所需权限 | `media.manage` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`record_id` 必填。

**成功响应**

`record_id`、`deleted=true`。

**补充说明**

- 注意：删除不可恢复；业务侧应二次确认。

#### 8.14.6 查询告警列表

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/alarms/list` |
| 功能说明 | 分页查询设备告警。 |
| 所需权限 | `alarm.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`cursor` 可选；`page_size` 1～200，默认 50；`task_name` 可选。

**成功响应**

`items[]`（Alarm）、`total`、`next_cursor`。

#### 8.14.7 查询告警详情

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/alarms/get` |
| 功能说明 | 读取一条告警详情。 |
| 所需权限 | `alarm.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`alarm_id` 必填，可传正整数或十进制字符串。

**成功响应**

Alarm。

#### 8.14.8 删除告警

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/alarms/delete` |
| 功能说明 | 删除告警记录。 |
| 所需权限 | `alarm.manage` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`alarm_id` 必填。

**成功响应**

`alarm_id`、`deleted=true`。

#### 8.14.9 查询日志列表

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/logs/list` |
| 功能说明 | 分页查询设备日志。 |
| 所需权限 | `log.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`cursor` 可选；`page_size` 1～100，默认 50；`source` 最长 128；`level` 最长 32；`keyword` 最长 256；`start_time`、`end_time` 最长 64。

**成功响应**

`items[]`（Log）、`total`、`next_cursor`。

**补充说明**

- 注意：过滤时间格式由设备日志源定义，集成时建议使用设备实际返回的时间格式。

#### 8.14.10 查询日志详情

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/logs/get` |
| 功能说明 | 读取一条日志。 |
| 所需权限 | `log.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`log_id` 必填，可传正整数或十进制字符串。

**成功响应**

Log。

### 8.15 人脸库 API

Face 写入对象字段：

| 字段 | 类型 | 必须 | 限制 |
|---|---|---|---|
| `face_id` | string | 是 | 1～128，只允许 ASCII 字母、数字、`_`、`-`、`.`、`:`（空格不允许） |
| `name` | string | 否 | 最长 128 字节 |
| `work_id` | string | 否 | 最长 128 字节 |
| `id_card_no` | string | 否 | 最长 128 字节 |
| `ic_card_no` | string | 否 | 最长 128 字节 |
| `department` | string | 否 | 最长 128 字节 |
| `phone` | string | 否 | 最长 64 字节 |
| `gender` | int | 否 | 0～2 |
| `age` | int | 否 | 0～150 |
| `category` | int | 否 | 0～2 |
| `photo_base64` | string | 注册/更新是 | 不含 `data:image/...;base64,` 前缀；编码后不超过 2,800,000 字节 |

#### 8.15.1 查询人脸分组

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/faces/folders` |
| 功能说明 | 列出人脸分组。 |
| 所需权限 | `face.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

`items[]`、`total`、`next_cursor`；当前默认分组项为 `folder_id`、`name`、`face_count`。

#### 8.15.2 查询人脸列表

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/faces/list` |
| 功能说明 | 分页读取人脸，不返回原始照片 Base64。 |
| 所需权限 | `face.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`cursor` 可选；`page_size` 1～32，默认 32。

**成功响应**

`items[]`（Face）、`total`、`next_cursor`。

**补充说明**

- 注意：翻页时保持同一 `page_size`，否则返回 `CURSOR_PAGE_SIZE_MISMATCH`。

#### 8.15.3 查询人脸详情

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/faces/get` |
| 功能说明 | 按编号查询人脸。 |
| 所需权限 | `face.read` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`face_id` 必填。

**成功响应**

Face；`photo_available` 仅表示设备有照片，不直接返回照片内容。

#### 8.15.4 注册人脸

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/faces/register` |
| 功能说明 | 异步注册单个人脸。 |
| 所需权限 | `face.write` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 202 Accepted` |

**请求参数**

完整 Face 写入对象，含 `photo_base64`。

**成功响应**

HTTP 202，Operation；成功结果为 `face_id`。

#### 8.15.5 更新人脸

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/faces/update` |
| 功能说明 | 异步全量更新人脸和照片。 |
| 所需权限 | `face.write` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 202 Accepted` |

**请求参数**

完整 Face 写入对象，当前版本更新时仍要求 `photo_base64`。

**成功响应**

HTTP 202，Operation；成功结果为 `face_id`。

#### 8.15.6 删除人脸

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/faces/delete` |
| 功能说明 | 删除人脸及受管图片。 |
| 所需权限 | `face.write` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`face_id` 必填。

**成功响应**

`face_id`、`deleted=true`。

#### 8.15.7 批量导入人脸

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/faces/batch_import` |
| 功能说明 | 异步批量注册人脸。 |
| 所需权限 | `face.write` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 202 Accepted` |

**请求参数**

`items[]`，1～8 个完整 Face 写入对象；批内 `face_id` 唯一；全部 Base64 合计不超过 3 MiB。

**成功响应**

HTTP 202，Operation。
`atomic=false`、`total`、`succeeded`、`failed`、`items[]`；每项为 `face_id`、`state`，失败项另含 `code`、`msg`。

**补充说明**

- 注意：批量导入非原子操作，部分成功时只重试失败项并使用新的幂等键。设备同时处理的人脸 Base64 有 32 MiB 有界预算，超限返回 429。

### 8.16 智能检索 API

#### 8.16.1 查询智能检索服务状态

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/retrieval/status` |
| 功能说明 | 读取检索服务、工作队列和索引的综合状态。 |
| 所需权限 | `retrieval.use` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

- 服务：`enabled`、`started`、`service_state_code`、`service_state_text`、`service_state_level`、`service_state_detail`、`service_state_hint`、`warming_up`、`bootstrap_failed`、`last_error`；
  - 队列：`worker_busy`、`active_job`、`job_queue_size`、`max_job_queue_size`；
  - 索引：`index_size`、`meta_size`、`image_meta_count`、`video_meta_count`、`indexing`、`index_dirty`、`pending_index_ops`、`last_index_flush_ms`、`last_search_snapshot_refresh_ms`；
  - 同步：`sync_queued`、`last_sync_stage`、`last_sync_detail`、`last_sync_started_ms`、`last_sync_finished_ms`、`total_sync_rounds`、`total_sync_failures`；
  - 运行配置：`run_location`、`runtime_location`、`infer_type`、`infer_device_id`、`feature_dim`，以及当前模型/数据库路径和阈值字段。

**补充说明**

- 状态对象可能随模型和设备版本增加诊断字段，客户端应忽略未知字段。

#### 8.16.2 查询检索索引状态

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/retrieval/index/status` |
| 功能说明 | 读取索引状态；当前返回与 `retrieval/status` 相同的完整快照，未来可单独演进。 |
| 所需权限 | `retrieval.use` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

同 `retrieval/status`。

#### 8.16.3 查询智能检索配置

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/retrieval/settings/get` |
| 功能说明 | 读取持久化配置、运行时配置和可选运行位置。 |
| 所需权限 | `retrieval.use` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

`success`、`config_path`、`config`、`persisted_config`、`run_location_options`、`runtime_location_options`、`warming_up`、`bootstrap_failed`；读取异常时可能含 `persisted_config_error`、`bootstrap_error`。

`config` 稳定字段：

`config_version`、`enabled`、`run_location`、`runtime_location`、`cold_search_page_size`、`hot_index_vectors`、`max_active_vectors`、`max_frames_per_video`、`max_frames_per_video_hard_limit`、`min_score_image`、`min_score_text`、`sample_frame_interval`、`video_frame_dedup_threshold`。


#### 8.16.4 更新智能检索配置

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/retrieval/settings/set` |
| 功能说明 | 持久化并应用检索配置。 |
| 所需权限 | `retrieval.use` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

非空配置对象；可直接传上述配置字段，或按设备返回结构传 `config` 对象。只修改确有需要的字段。

**成功响应**

`success`、`saved`、`applied`、`applying`、`message`、`config_path`、`config`、`runtime_config`、`run_location_options`、`runtime_location_options`；失败时可能含 `apply_error`。

**补充说明**

- 注意：模型/运行位置切换可能异步预热；随后轮询 `retrieval/status`，直到 `warming_up=false` 或明确失败。

#### 8.16.5 触发检索索引同步

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/retrieval/sync` |
| 功能说明 | 触发媒体元数据与向量索引同步。 |
| 所需权限 | `retrieval.use` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`{}` 可触发默认同步；设备版本支持的附加同步选项会在响应或能力中给出，不应自行构造未知字段。

**成功响应**

同步受理/队列状态，常见字段为 `success`、`queued`、`message` 及当前同步阶段。

**补充说明**

- 后续：轮询 `retrieval/status` 的 `sync_queued,indexing,last_sync_*`，不要持续重复提交同步。

#### 8.16.6 执行智能检索

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/retrieval/query` |
| 功能说明 | 按自然语言检索图片/录像。 |
| 所需权限 | `retrieval.use` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

`query` 必填（1～2048 字节）；`top_k` 1～200，默认 20；`media_scope` 为 `all`、`image`、`video`。

**成功响应**

`success`、`results[]` 以及设备返回的耗时/分页诊断字段。

**补充说明**

- `results[]` 稳定字段：匹配分数 `score`、来源标识/类型、`media_reference`；有预览时含 `preview_reference`，并可能包含任务、时间、帧号、录像偏移等媒体元数据。
- 注意：`media_reference`/`preview_reference` 是受控媒体引用，不应假定是设备绝对文件路径；服务未就绪时按返回的 `retryable` 和状态提示处理。

### 8.17 在线升级 API

#### 8.17.1 检查可用升级

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/upgrades/check` |
| 功能说明 | 刷新并获取应用/固件可用版本。 |
| 所需权限 | `upgrade.manage` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

顶层字段：

| 字段 | 类型 | 说明 |
|---|---|---|
| `deviceId` | string | 设备编号 |
| `company` | string | 厂商标识 |
| `current` | object | 当前应用与固件版本 |
| `capabilities` | object | 设备升级能力 |
| `check` | object | 本次检查状态和升级源信息 |
| `app` | object | 应用升级候选 |
| `firmware` | object | 固件升级候选 |
| `runningTask` | object | 当前运行任务摘要，用于升级前置检查 |

`current` 与 `capabilities`：

| 对象 | 字段 | 类型 | 说明 |
|---|---|---|---|
| `current` | `appVersion` | string | 当前应用版本 |
| `current` | `firmwareVersion` | string | 当前固件版本 |
| `capabilities` | `appTarget` | string | 应用安装目标 |
| `capabilities` | `firmwareTarget` | string | 固件安装目标 |
| `capabilities` | `supportsFirmwareUpgrade` | bool | 是否支持固件在线升级 |

`check`：

| 字段 | 类型 | 说明 |
|---|---|---|
| `status` | string | 检查状态 |
| `message` | string | 状态说明 |
| `checkedAt` | string/int64 | 最近检查时间 |
| `autoCheckEnabled` | bool | 是否启用自动检查 |
| `autoCheckIntervalSec` | int | 自动检查周期，单位秒 |
| `checkUrl` | string | 检查接口地址 |
| `manifestUrl` | string | 升级清单地址 |

`app` / `firmware` 候选对象：

| 字段 | 类型 | 说明 |
|---|---|---|
| `kind` | string | `app` 或 `firmware` |
| `supported` | bool | 当前设备是否支持该升级类型 |
| `currentVersion` | string | 当前版本 |
| `latestVersion` | string | 最新版本 |
| `hasUpdate` | bool | 是否存在可用升级 |
| `releaseId` | string | 发布版本编号，供 `upgrades/apply` 使用 |
| `releaseNotes` | string | 版本说明 |
| `publishedAt` | string/int64 | 发布时间 |
| `size` | int64 | 安装包字节数 |
| `sha256` | string | 安装包 SHA-256 |
| `mandatory` | bool | 是否为强制升级 |
| `phase` | string | 当前阶段 |
| `progress` | number | 进度百分比 |
| `message` | string | 状态说明 |
| `checkedAt` | string/int64 | 检查时间 |

设备版本可能在候选对象中增加下载目标等可选字段，客户端必须忽略未知字段。

#### 8.17.2 查询升级状态

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/upgrades/status` |
| 功能说明 | 读取当前升级进度。 |
| 所需权限 | `upgrade.manage` |
| 幂等键 | 不需要 |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

| 字段 | 类型 | 说明 |
|---|---|---|
| `active` | bool | 是否存在活动升级 |
| `kind` | string | `app` 或 `firmware` |
| `releaseId` | string | 发布版本编号 |
| `phase` | string | 当前阶段，枚举见下表 |
| `progress` | number | 进度百分比 |
| `currentVersion` | string | 当前版本 |
| `targetVersion` | string | 目标版本 |
| `message` | string | 当前状态或失败原因 |
| `startedAt` | string/int64 | 开始时间 |
| `updatedAt` | string/int64 | 更新时间 |
| `finishedAt` | string/int64 | 完成时间 |
| `filename` | string | 安装包文件名 |
| `sha256` | string | 安装包 SHA-256 |

`phase` 枚举：

| 值 | 含义 |
|---|---|
| `idle` | 无升级任务 |
| `pending` | 等待执行 |
| `downloading` | 正在下载 |
| `verifying` | 正在校验安装包 |
| `installing` | 正在安装 |
| `restarting` | 正在重启相关服务或设备 |
| `success` | 升级成功 |
| `failed` | 升级失败，查看 `message` |
| `cancelled` | 已取消 |

设备版本可能增加下载来源、安装校验和退出码字段。

#### 8.17.3 启动在线升级

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/upgrades/apply` |
| 功能说明 | 启动应用或固件升级。 |
| 所需权限 | `upgrade.manage` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

| 字段 | 类型 | 必须 | 说明 |
|---|---|---|---|
| `kind` | string | 是 | `app` 或 `firmware` |
| `release_id` | string | 否 | 发布版本编号，建议使用 `upgrades/check` 返回值 |

**成功响应**

| 字段 | 类型 | 说明 |
|---|---|---|
| `upgrade` | object | 当前升级状态，字段同 8.17.2 |

**补充说明**

- 安全流程：先检查设备能力、目标版本、SHA-256 和运行任务；升级期间持续查询状态，不能因一次 HTTP 超时重复生成新幂等键。

#### 8.17.4 取消在线升级

| 接口属性 | 说明 |
|---|---|
| 请求地址 | `POST /openapi/v1/upgrades/cancel` |
| 功能说明 | 取消仍处于可取消阶段的升级。 |
| 所需权限 | `upgrade.manage` |
| 幂等键 | 必须提供 `idempotency_key` |
| 成功状态 | `HTTP 200 OK` |

**请求参数**

无业务参数，`param` 传空对象 `{}`。

**成功响应**

| 字段 | 类型 | 说明 |
|---|---|---|
| `upgrade` | object | 取消后的升级状态，字段同 8.17.2 |

**补充说明**

- 注意：进入安装/重启阶段后可能不可取消；此时按服务端错误和当前状态处理。

## 9. 分页、幂等和重试

### 9.1 分页

```json
{"page_size":50,"cursor":"aibox-v1-50"}
```

首次请求不传 `cursor`。后续直接使用响应中的 `next_cursor`，不得自行解析或修改。

### 9.2 必须提供幂等键的写接口

```text
tasks/create, tasks/save, tasks/delete, tasks/clone
tasks/start, tasks/stop, tasks/restart, tasks/apply_and_start
operations/cancel
records/delete, alarms/delete
faces/register, faces/update, faces/delete, faces/batch_import
upgrades/apply, upgrades/cancel
retrieval/settings/set, retrieval/sync
```

同一次业务请求因超时重试时，必须使用相同 `idempotency_key`。业务参数变化时必须生成新 Key。

### 9.3 重试建议

| 情况 | 处理方式 |
|---|---|
| 网络超时、明确 `retryable=true` | 指数退避后重试 |
| `40101` | 重新获取 Access Token |
| `40301` | 申请所需 Scope，不要重试 |
| `40901` | 重新读取任务并处理 Revision 冲突 |
| `40904` | 放弃旧 cursor，重新获取快照并订阅 |
| `40905` | 重新读取资源状态后再决定操作，不执行事件游标恢复 |
| `42901` | 等待服务端建议时间并降低并发 |
| 其他 4xx | 修正请求后再调用 |

推荐普通请求超时 10 秒，录像/日志/节点目录 30 秒。异步接口只等待 HTTP `202`，后续轮询 Operation，不要保持长 HTTP 请求。HTTP `429` 会同时返回 JSON `retry_after_ms` 和标准 `Retry-After` Header；客户端必须取两者中更保守的等待时间并加入随机抖动。

### 9.4 第三方入口资源预算

第三方 OpenAPI 与本地网页、CloudWS 使用独立调度预算。当前单设备默认值：

| 资源 | 第三方 OpenAPI 默认上限 |
|---|---:|
| 同时执行请求 | 全局 4、单 Client 2 |
| 请求速率 | 单 Client 60 次/秒 |
| 写操作速率 | 单 Client 10 次/秒 |
| 活跃 Access Token | 全部 OpenAPI Client 合计 2048；单 Client 16 |
| 异步 Operation | 独立队列 32；单 Client 最多 8 个未完成 |
| Operation 工作线程 | 独立 1 条，不占用已有控制工作线程 |
| 事件订阅 | OpenAPI 合计 128；单认证主体 8 |

达到上限返回 `42901`，不会排队占用本地网页或 CloudWS 的控制资源。交付版本可以降低这些上限；客户端不得依赖高频轮询代替事件订阅。

## 10. 错误码

| code | HTTP | 含义 |
|---:|---:|---|
| `0` | 200/202 | 成功或异步受理 |
| `40001` | 400 | 参数错误 |
| `40002` | 413 | 请求体超过 4 MiB |
| `40101` | 401 | Token 或客户端凭证无效 |
| `40301` | 403 | Scope 不足或鉴权域不匹配 |
| `40401` | 404 | 资源不存在或不可见 |
| `40402` | 404 | URI 未开放 |
| `40901` | 409 | Task Revision 冲突 |
| `40902` | 409 | 幂等键与原请求参数不一致 |
| `40903` | 409 | Operation 当前不可取消 |
| `40904` | 409 | Event cursor 已过期 |
| `40905` | 409 | 资源当前状态不允许该操作 |
| `42201` | 422 | 任务图或节点配置校验失败 |
| `42202` | 422 | 任务运行时或依赖尚未满足 |
| `42901` | 429 | 频率、队列或内存预算超限 |
| `50001` | 500 | 设备内部错误 |
| `50002` | 500 | 服务重启导致 Operation 中断 |
| `50301` | 503 | 依赖服务未就绪 |
| `52001` | 503 | 媒体源不可用 |
| `52002` | 503 | 设备资源不足 |

## 11. 安全要求

- 生产环境必须使用可信 HTTPS/WSS 证书，不得跳过证书校验。
- 每个第三方系统使用独立 `client_id/client_secret` 和最小 Scope。
- `client_secret` 和 Access Token 不得写入 URL、日志或分析埋点；WSS/录像 Ticket 只能放在协议规定的临时 URL 中，日志必须脱敏。
- `client_secret` 泄露后立即轮换；轮换后旧密钥和该客户端已有 Access Token 立即失效。
- 本地网页会话、第三方客户端密钥和云端设备身份属于三个独立鉴权域，凭证不能互换。
- 第三方 OpenAPI Token 不能调用本地网页私有 URI，也不能作为 CloudWS 设备身份；公开 WSS 在 HTTPS Ticket 建立时固定认证主体，消息体不能切换 Token。
- OpenAPI 的限流、事件订阅和异步 Operation 使用独立配额。OpenAPI 过载或队列满只返回该调用的 `42901`，不得阻塞本地网页登录、CloudWS 心跳或已有任务运行线程。
- `operation.state.changed` 按创建 Operation 时固定的第三方客户端主体私有投递；客户端之间不能互看，通配管理账号也不会被动收到其他客户端的私有事件。
- 人脸、升级和检索事件采用字段白名单，不能作为下载文件、人脸图片、特征向量或检索结果的数据通道。
- 人脸图片属于敏感个人信息，接入方必须取得合法授权并控制保存期限和访问范围。

## 12. 版本兼容

客户端必须忽略响应中不认识的新增字段。`OpenAPI v1` 可以增加可选字段、节点类型和新 URI，但不会改变已发布字段的既有含义。

文档以本版本列出的接口为正式对接边界；未列出的设备网页私有 URI 不承诺兼容性。
