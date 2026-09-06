# AIBox 第三方开放 API 对接协议

| 文档属性 | 内容 |
|---|---|
| 文档名称 | AIBox 第三方开放 API 对接协议 |
| 文档版本 | `1.4.9` |
| 协议版本 | `OpenAPI v1` |
| 发布日期 | `2026-09-06` |
| 适用对象 | 第三方平台、客户自研 Web/服务端应用 |
| English version | [AIBOX_OPENAPI_INTEGRATION_PROTOCOL_EN.md](../../en/reference/openapi-protocol.md) |

## 修订记录

| 版本 | 日期 | 变更摘要 |
|---|---|---|
| `1.4.9` | `2026-09-06` | 补充客户 WSS 错误帧处理、有限退避及历史证书兼容说明；业务接口不变 |
| `1.4.8` | `2026-09-06` | 仅补用户名密码自助领取 Client 凭据和改密失效；保留原 Client Credentials 与业务协议 |
| `1.4.6` | `2026-09-06` | 增加 Python/Node.js/curl 客户可运行示例；修正拉流参数为 rtsp_url；修改任务示例保留原图，补充测试与交付边界；业务协议未改变 |
| `1.4.5` | `2026-08-03` | 补全 18 个实时事件契约、Token/WSS 续期、事件补偿与订阅续期、能力边界、幂等/Operation 保留期及客户验收清单 |

> **调用示例下载**：[Python / Node.js / curl 示例包 1.4.9](../../assets/downloads/aibox-openapi-examples-1.4.9.zip)（[SHA-256 校验文件](../../assets/downloads/aibox-openapi-examples-1.4.9.zip.sha256)）。包内 `TaskManager/examples/openapi/README.md` 提供逐步操作说明，覆盖凭据领取、首次连接、任务创建/修改/启停、事件和录像下载。

> **开放边界**：本文只说明客户如何连接设备、发送请求和处理响应。AIBox 内部网页接口、数据库和插件文件不属于开放协议。

> **传输边界**：本文是“设备直连分册”，适用于客户系统能够通过局域网、专网、VPN 或私有 APN 访问盒子的场景。盒子位于 NAT/4G/普通互联网后时，应由盒子主动建立到 AIBox Device Gateway 的 WSS 长连接；客户平台调用 Gateway 北向 API，不得把设备 `8099` 端口直接暴露到公网。设备上行协议、Gateway API 与本协议共享业务 URI 和数据模型，但使用独立的设备身份、租户授权和连接会话。

## 文档导航

| 阅读目标 | 对应章节 |
|---|---|
| 完成首次连接和鉴权 | 第 1～2 章 |
| 了解 HTTP/WSS 报文格式 | 第 3～4 章 |
| 创建、编辑、启停任务 | 第 5 章 |
| 接收实时告警、任务/节点状态和设备状态 | 第 6 章 |
| 获取媒体输出、录像和告警历史 | 第 7 章、第 8.14 节 |
| 查询全部 API | 第 8 章 |
| 处理分页、重试和错误 | 第 9～10 章 |
| 安全和版本兼容要求 | 第 11～12 章 |
| 确认当前版本能力边界 | 第 13 章 |
| 执行客户交付验收 | 第 14 章 |

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

设备直连 HTTPS 不承诺支持任意浏览器跨域调用。客户网页应请求自己的 BFF，再由 BFF 调用设备 OpenAPI。确需浏览器直接建立设备 WSS 时，浏览器 `Origin` 必须与设备同源，或由交付方在设备配置中加入精确的 `scheme://host:port` 白名单；禁止配置 `*`。设备证书必须被浏览器信任，并覆盖实际访问域名或 IP。

每个第三方系统使用独立的凭据：

```text
client_id     公开标识，例如 aibc_xxx
client_secret 私密密钥，例如 aibsk_xxx，仅展示一次
```

**可直接用设备网页登录用户名和密码自助领取。** 通过 HTTPS `POST /openapi/v1/command` 调用新增的 `/openapi/v1/clients/bootstrap`，从 `result.client_id`、`result.client_secret` 获取凭据。原有 `auth/token` 换 Token 和后续业务接口不变；不增加另一套账号 Token 模式。原管理员 `clients/create` 入口继续保留，`clients/get/list` 不返回原密钥。

#### 1.1.1 自助领取（仅首次或改密后）

接口索引：`POST /openapi/v1/clients/bootstrap`。这是 Command URI；实际 HTTP 请求仍发到 `/openapi/v1/command`。

不带 `X-Access-Token`，请求示例：

```json
{"uri":"/openapi/v1/clients/bootstrap","request_id":"bootstrap-unique-001","param":{"username":"<网页账号>","password":"<原始密码>","display_name":"客户平台","scopes":["device.read","task.read"]}}
```

成功 `code=0`，`result` 包含 `client_id`、`client_secret`、`secret_returned_once:true` 和 Client 元数据，不包含 Access Token。`display_name` 可省略；`scopes` 可省略（使用该账号允许的业务 Scope），建议明确申请最小权限，不能申请超出账号授权或密钥管理权限。完整 Python/Node.js/curl 领取程序及使用步骤见[示例包](../../assets/downloads/aibox-openapi-examples-1.4.9.zip)内的 README。

修改网页密码、账号删除/重建或权限变化，会使该账号自助领取的旧 Client 凭据及派生 Token 失效，**改回旧密码也不能复活**。必须用新密码重新领取；WSS 后续收发/心跳不再接受旧身份，重连时按持久化游标补偿。已经接受的业务操作不因此自动取消。原独立管理员 Client 不受无关账号改密影响。

同账号同 `request_id` 仅签发一次；重复返回 `40901 / CLIENT_SECRET_ALREADY_ISSUED`，不会重发 Secret。响应丢失应凭请求 ID 核查，不要自动循环新建。每账号当前版本最多 16 个启用自助 Client、设备最多 4096 条自助记录，超限 `40901 / ACCOUNT_CLIENT_LIMIT`。错误密码 `40101`，越权 `40301`，限流 `42901`，存储暂不可读 `50301`（拒绝访问但不误撤销凭据）。用户名/密码只放 HTTPS POST JSON，不放 URL 或日志。

`client_secret` 只用于换取短期 Access Token，不得放入 URL、网页前端代码或普通日志。

第三方 `openapi_client` Access Token 只能放在 `X-Access-Token`。URL Query、Cookie、JSON 顶层 `token` 和 `param.access_token` 均不属于 OpenAPI 鉴权方式。设备内部旧网页的兼容 Token 解析不构成对外协议。

### 1.2 Scope 权限清单

交付方按客户实际功能签发最小权限，不应默认授予全部 Scope：

| Scope | 允许的能力 |
|---|---|
| `device.read` | 设备身份、授权、版本、资源、网络和运行位置只读查询；订阅设备状态事件 |
| `task.read` | 任务、节点、节点目录和插件状态只读查询；订阅任务/节点/插件事件 |
| `task.write` | 创建、保存、复制、删除任务和校验任务图 |
| `task.execute` | 启动、停止、重启任务 |
| `media.read` | 媒体输出、录像列表/详情/下载；订阅录像事件 |
| `media.manage` | 删除录像；通常应与 `media.read` 配合授予 |
| `alarm.read` | 告警列表/详情；订阅实时告警事件 |
| `alarm.manage` | 删除告警；通常应与 `alarm.read` 配合授予 |
| `log.read` | 查询设备日志 |
| `face.read` | 查询人脸分组和人员元数据；订阅人脸库变化事件 |
| `face.write` | 注册、更新、删除和批量导入人脸 |
| `retrieval.use` | 检索状态、设置、索引同步和查询；订阅检索事件 |
| `upgrade.manage` | 检查、启动、取消升级及订阅升级事件 |
| `openapi.clients.manage` | 设备本地交付管理员管理第三方凭证；禁止授予第三方 Client |

`task.write` 不隐含 `task.read`，`alarm.manage` 不隐含 `alarm.read`，其他 Scope 也不会自动继承。客户需要同时读写时，必须显式申请两者。

### 1.3 Token 生命周期与设备时间

本协议不签发 Refresh Token。客户服务端应缓存 Access Token，并在 `expires_at` 前 5 分钟或剩余有效期不足 10% 时（取较早者）重新调用 `auth/token`。不要为每次业务请求重新申请 Token；单 Client 默认最多同时保留 16 个有效 Token。

公开 WSS 在连接建立时绑定签发 Ticket 所使用的 Access Token。长连接续期流程为：申请新 Token → 申请新 Ticket → 建立新 WSS → 使用已持久化的事件 `cursor` 新建订阅并完成补偿 → 关闭旧 WSS。收到 `40101`、密钥轮换、Scope 修改或 Client 被禁用后，必须停止使用旧连接和旧 Token；不得因为旧 WSS 尚未物理断开而继续认为其授权有效。

`server_time_ms`、带 `_ms` 后缀的字段和事件 `occurred_at_ms` 均为 Unix 毫秒。设备与客户平台必须启用可靠的 NTP/授时；客户端可用 `server_time_ms` 监测时钟偏差，但不得用本地接收时间覆盖设备业务时间。升级模块沿用的少数无 `_ms` 时间字段见第 8.17 节说明。

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
| `request_id` | 建议 | 客户端生成的请求追踪 ID；建议使用 1～128 个可打印 ASCII 字符并在单次调用中唯一 |
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

服务端在未提供 `request_id` 时会生成追踪值，但客户平台不能依赖该行为做端到端链路追踪。重试同一次业务调用时保留原 `request_id` 和 `idempotency_key`；发起新的业务调用时生成新的 `request_id`。

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

当前不要求设置 `Sec-WebSocket-Protocol`；响应中的 `protocol=aibox.openapi.v1` 是应用层包络版本。仅发送 UTF-8 文本 JSON，服务端不协商 `permessage-deflate`，消息大小按未压缩 JSON 字节数计算。

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

客户端应按 `ready.heartbeat_interval_ms` 发送应用层 `ping`，连续两个周期未收到 `pong` 或任何有效响应时主动断开并重新申请 Ticket。不要在旧连接上无限重试。

WSS 约束与关闭语义：

| 条件 | 服务端行为 | 客户端处理 |
|---|---|---|
| Ticket 无效、过期、复用或连接数超限 | 关闭码 `1008` | 重新获取 Token/Ticket；连接数超限时先关闭旧连接 |
| 发送二进制消息 | 关闭码 `1003` | 改用 UTF-8 JSON 文本 |
| 单条消息超过 4 MiB | 关闭码 `1009` | 缩小请求；大型文件不得通过 WSS 传输 |
| 请求速率超过 100 条/秒 | 关闭码 `1008` | 降低并发并指数退避重连 |
| 客户端消费过慢、发送缓冲达到上限 | 关闭码 `1013` | 先通过 `events/poll` 补偿，再降低订阅量或提高消费能力 |

同一认证主体最多同时保持 4 条公开 WSS 连接、持有 8 张未消费 Ticket。多标签页或多实例必须共享连接或做好连接配额管理。

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
          "config": {"rtsp_url": "rtsp://192.168.1.20/live"}
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
        "config": {"rtsp_url": "rtsp://192.168.1.20/live"}
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
      "nodes": [
        {
          "node_id": "input-1",
          "node_type": "netclient",
          "category": "custom-input",
          "node_schema_version": 1,
          "config": {"rtsp_url": "rtsp://192.168.1.20/live"}
        }
      ],
      "edges": []
    }
  }
}
```

`tasks/save` 是完整替换，不是局部 Patch。上例保留第 5.2 节的原节点；实际调用必须复制 `tasks/get` 返回的完整节点、连线和配置后修改，不能用空数组表示“没有修改节点”。版本冲突返回：

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

### 6.1 实时上报能力

第三方平台建立 WSS 后，可订阅实时告警、任务/节点状态变化和设备控制面状态变化。事件用于及时通知，业务详情以权威读取接口为准：

| 业务数据 | 实时通知 | 权威读取接口 | 当前语义 |
|---|---|---|---|
| 新告警 | `alarm.created` | `alarms/get` | 告警记录及快照写入成功后通知 |
| 告警删除 | `alarm.deleted` | 无；从客户缓存删除 | 告警记录删除后通知 |
| 任务运行状态 | `task.state.changed` | `tasks/runtime` | 启动、停止、重启及失败等状态变化 |
| 节点状态 | `node.state.changed` | `tasks/node_metrics` | 节点状态快照可能变化，收到后刷新整份任务节点快照 |
| 设备控制面状态 | `device.state.changed` | `device/get` | 设备到 AIBox 云控制面的 `offline/connecting/online` 变化 |
| 设备资源指标 | 无周期事件 | `device/metrics` | CPU、NPU、内存、温度和存储按需采集 |

> **在线状态边界**：客户平台直连盒子时，应以自身 WSS 是否连接、`ping/pong` 是否按时返回判断盒子对该平台是否在线。`device.state.changed` 表示盒子到 AIBox 云控制面的状态，不能替代客户平台自身的连接状态。

> **节点指标边界**：当前 `node.state.changed` 是事件驱动的状态失效通知，不是逐节点周期遥测。`tasks/node_metrics` 当前提供任务运行态聚合到节点的状态；真实插件级 FPS、处理延迟和丢帧指标尚未开放，服务端会明确返回 `metrics_available=false`，客户不得将其解释为节点故障。

### 6.2 支持的 Topic

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
| `node.state.changed` | `task.read` | `task_nodes` | 某任务的节点状态快照可能变化 |
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

### 6.3 订阅实时告警、节点和设备状态

推荐在同一个 WSS 会话中一次订阅需要的 Topic：

订阅：

```json
{
  "uri": "/openapi/v1/events/subscribe",
  "param": {
    "topics": [
      "alarm.created",
      "alarm.deleted",
      "task.state.changed",
      "node.state.changed",
      "device.state.changed"
    ]
  }
}
```

响应：

```json
{
  "code": 0,
  "msg": "OK",
  "result": {
    "subscription_id": "sub_xxx",
    "topics": [
      "alarm.created",
      "alarm.deleted",
      "task.state.changed",
      "node.state.changed",
      "device.state.changed"
    ],
    "delivery": "realtime+poll",
    "replay_items": [],
    "next_cursor": "evc_xxx:100",
    "expires_in_ms": 1800000
  }
}
```

通过 WSS 订阅时 `delivery` 为 `realtime+poll`；通过 HTTPS 订阅时为 `poll`。上述订阅同时需要 `alarm.read`、`task.read`、`device.read`；缺少任一 Scope 时整个订阅返回 HTTP 403，不会建立部分订阅。

### 6.4 实时告警事件

告警记录和告警图片写入成功后，服务端发送：

```json
{
  "type": "event",
  "subscription_id": "sub_xxx",
  "event_id": "evt_xxx_101",
  "sequence": 101,
  "cursor": "evc_xxx:101",
  "topic": "alarm.created",
  "occurred_at_ms": 1785600000000,
  "resource": {"type": "alarm", "id": "1024"},
  "data": {
    "task_id": "task-001",
    "alarm_type": "intrusion",
    "image_available": true
  }
}
```

处理顺序：

1. 使用 `resource.id` 作为 `alarm_id` 调用 `alarms/get`，取得 `task_name`、`summary`、`occurred_at` 等权威字段。
2. 按 `event_id` 去重并完成客户业务落库。
3. 业务落库成功后调用 `events/ack`；不能在业务处理前提前 ACK。
4. `alarm.deleted` 到达后，从客户缓存删除对应 `resource.id`；再次调用 `alarms/get` 可能返回 404。

事件正文不携带图片二进制或设备内部文件路径，避免 WSS 大报文和路径泄露。当前开放 Alarm 模型只声明 `image_available`；若项目要求第三方下载告警原图，应在交付能力清单中确认安全图片 Ticket 接口，不得拼接设备内部路径或依赖本地网页静态地址。

### 6.5 节点状态变化事件

任务启动、停止、重启或异步执行失败等导致节点状态快照可能变化时，服务端发送：

```json
{
  "type": "event",
  "subscription_id": "sub_xxx",
  "event_id": "evt_xxx_102",
  "sequence": 102,
  "cursor": "evc_xxx:102",
  "topic": "node.state.changed",
  "occurred_at_ms": 1785600000100,
  "resource": {"type": "task_nodes", "id": "task-001"},
  "data": {
    "task_id": "task-001",
    "state": "running",
    "reason": "task-started",
    "snapshot_required": true,
    "snapshot_uri": "/openapi/v1/tasks/node_metrics"
  }
}
```

收到后调用 `tasks/node_metrics`，请求参数为 `{"task_id":"task-001"}`，一次刷新该任务全部节点。不要为每个节点分别轮询，也不要把事件中的 `state` 当作所有节点最终状态。

### 6.6 设备状态变化与资源信息

盒子到 AIBox 云控制面的归一化连接状态变化时，服务端发送：

```json
{
  "type": "event",
  "subscription_id": "sub_xxx",
  "event_id": "evt_xxx_103",
  "sequence": 103,
  "cursor": "evc_xxx:103",
  "topic": "device.state.changed",
  "occurred_at_ms": 1785600000200,
  "resource": {"type": "device", "id": "AX650X_xxx"},
  "data": {
    "state": "online",
    "control_plane_state": "connected",
    "detail": "connected"
  }
}
```

`state` 稳定取值为 `offline`、`connecting`、`online`；`control_plane_state` 和 `detail` 用于诊断，客户端不得依赖其自由文本枚举。CPU、NPU、内存、CMM、温度和存储不会随该事件推送，客户监控页应调用 `device/metrics`，前台建议 3～5 秒一次并加入 10%～20% 抖动，页面隐藏或进入后台后暂停或降频。

### 6.7 可靠交付与断线补偿

事件是至少一次交付。客户端必须：

1. 按 `event_id` 去重。
2. 按 `sequence` 维护连续处理位置；并发产生的事件可能先后到达，不能越过尚未处理的事件提前 ACK。
3. 业务落库后调用 `events/ack`，并保存最后连续成功处理的 `cursor`。
4. `events/subscribe` 一次最多返回 100 条 `replay_items[]`。订阅建立后继续调用 `events/poll`，直到 `has_more=false`；补偿期间先缓存新到达的实时事件，补偿完成后再按 `sequence` 合并处理。
5. WSS 断线后，用相同 Topic 和最后成功处理的 `cursor` 重新调用 `events/subscribe`，完成补偿后再恢复实时消费。

客户端不得把 WSS 在线等同于状态永远最新。首次连接、重连、游标过期 `40904` 或事件声明 `snapshot_required=true` 时，必须调用上表的权威读取接口重建本地快照。

WSS 关闭时，与该连接绑定的 `subscription_id` 会立即失效；旧 ID 不能跨连接恢复，也不能在断线后直接用于 HTTPS `events/poll`。跨连接恢复依据是持久化的 `cursor`。如果需要在 WSS 断开期间使用 HTTPS 补偿，应先通过 HTTPS 使用 `topics + cursor` 新建 `delivery=poll` 订阅，再对新 `subscription_id` 调用 `events/poll`。

订阅空闲 30 分钟会被回收；WSS `ping/pong` 只保活连接，不刷新订阅 TTL。即使 Topic 长时间没有事件，客户端也应至少每 20 分钟对该订阅调用一次 `events/poll`，或在原 WSS 上携带相同 `subscription_id`、Topic 和最后成功 `cursor` 重新调用 `events/subscribe`。收到 `40401` 表示订阅已不存在，应使用持久化 `cursor` 新建订阅。

设备事件日志最多保留 1024 条且总计不超过 4 MiB，达到任一限制即淘汰最旧记录；它没有固定的时间保留承诺。离线时间较长时必须允许 `40904`，并通过权威快照完成全量恢复。

### 6.8 客户平台启动与恢复顺序

客户平台每次启动或重新连接设备时，建议严格按以下顺序执行：

1. 通过 HTTPS 获取 Access Token，并调用 `session/get` 确认 Scope。
2. 申请一次性 WSS Ticket，建立 WSS，等待 `ready`。
3. 立即订阅所需 Topic；先处理响应中的 `replay_items[]`，再处理实时事件。
4. 并行读取 `device/get`、`device/metrics`、`tasks/list`；对关注任务读取 `tasks/runtime` 和 `tasks/node_metrics`。
5. 首次同步告警时调用 `alarms/list`；随后使用 `alarm.created/alarm.deleted` 增量维护。
6. 按 `event_id` 去重；按 `sequence` 连续处理，业务落库后 ACK 并持久化 `next_cursor`。继续 `events/poll` 直到 `has_more=false`，再合并补偿期间缓存的实时事件。
7. 每 20 分钟以内刷新订阅活动时间。WSS 断开时采用指数退避重连；使用已保存的 `cursor` 建立新订阅并完成补偿。需要 HTTPS 补偿时先新建 poll 订阅，不能复用已经随连接释放的旧 `subscription_id`。收到 `40904` 时放弃旧游标，重新读取全部权威快照后建立新订阅。

不要先读取快照、数秒后才订阅事件，否则快照读取与订阅建立之间存在丢失状态变化的窗口。

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

当前实现共开放 `65` 个 URI（其中任务启动、停止由同一注册器动态生成）。8.1～8.7 提供快速索引，8.8～8.17 给出数据模型和逐接口说明。

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

例外：新增 `clients/bootstrap` 是客户凭网页账号密码调用的自助领取入口，不需要管理 Token，详见 1.1.1。以下原有管理接口和权限边界保持不变。

除自助领取外，第三方客户通常不调用本组管理接口。交付方使用具有 `openapi.clients.manage` Scope 的管理 Token 创建、轮换或禁用客户凭证。

`openapi.clients.manage` 和通配符 `*` 是本地交付管理员的保留权限，不能写入第三方客户端的 `scopes[]`。服务端同时禁止任何 `token_use=openapi_client` 的 Token 调用以下原有管理接口；即使旧版本数据库中存在误配 Scope，也不会放行。自助领取只验证网页账号密码，不接受此 Token 代替密码。

| URI | param | result |
|---|---|---|
| `/openapi/v1/clients/bootstrap` | `username`、`password`、`display_name?`、`scopes?`；HTTPS POST、无需管理 Token | Client 和仅返回一次的 `client_secret`，详见 1.1.1 |
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

创建和轮换均不是可盲目重试的幂等接口，响应中的 `client_secret` 只返回一次：轮换响应丢失时，对已知 `client_id` 再执行一次轮换；创建响应丢失时，先通过 `clients/list` 核对并禁用可能已创建的孤立 Client，再显式创建新的 Client。设备不能查询原密钥。

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
- 请求超时不能直接重试创建。先调用 `clients/list` 核对是否已经产生 Client，禁用无法取得密钥的孤立记录后再创建。

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
- 若轮换成功响应在网络中丢失，可对同一 `client_id` 再轮换一次并只保存最后一次返回的密钥；不要继续尝试旧密钥。

### 8.10 Operation API

异步写接口返回 HTTP 202 和 Operation。Operation 只对提交它的认证主体可见；具有通配管理权限的管理员除外。

Operation 记录当前在最后更新时间后保留 7 天。客户平台应在提交后立即保存 `operation_id`，通过 `operation.state.changed` 或轮询取得终态，并把业务结果写入自己的持久化存储；不得把设备 Operation 表当作长期审计库。

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

事件公共字段：

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | string | WSS 推送固定为 `event` |
| `subscription_id` | string | 产生本次投递的订阅；HTTPS `poll` 的 `items[]` 中可能省略 |
| `event_id` | string | 全局事件标识；用于幂等去重 |
| `sequence` | int64 | 当前事件日志内单调递增序号，不保证跨服务重启连续 |
| `cursor` | string | 不透明恢复位置；客户端只能保存和原样传回 |
| `topic` | string | 事件 Topic |
| `occurred_at_ms` | int64 | 事件产生时间；不一定等于告警业务发生时间 |
| `resource.type` | string | 资源类型，例如 `alarm`、`task_nodes`、`device` |
| `resource.id` | string | 资源标识；告警为 `alarm_id`，节点事件为 `task_id` |
| `data` | object | Topic 对应的轻量通知字段，不替代权威资源快照 |

各 Topic 的 `data` 稳定字段如下；标记“可选”的字段只在相应触发路径出现：

| Topic | `data` 字段 | 说明 |
|---|---|---|
| `alarm.created` | `task_id,alarm_type,image_available` | `image_available` 仅表示有关联图片，图片本身不进入事件 |
| `alarm.deleted` | `task_id` | `resource.id` 为被删除的 `alarm_id` |
| `record.created`、`record.deleted` | `task_id,filename` | `filename` 仅为安全文件名，不含设备目录；详情读取 `records/get` |
| `task.created`、`task.updated` | `task_id,task_name,revision,desired_state,updated_at_ms,snapshot_required` | `snapshot_required=true`，随后读取 `tasks/get` |
| `task.deleted` | `task_id,deleted,snapshot_required` | `deleted=true`、`snapshot_required=false` |
| `task.state.changed` | TaskRuntime 字段及 `reason`，或 `operation_id,operation_kind,state,code,msg,result?,error?` | 事件可能由直接运行态变化或异步 Operation 产生；统一以 `tasks/runtime` 为权威状态 |
| `node.state.changed` | `task_id,state,snapshot_required,snapshot_uri,reason?,operation_id?` | `reason` 与 `operation_id` 取决于触发路径；必须整体读取 `tasks/node_metrics` |
| `plugin.catalog.changed` | `reason,revision,ready,discovered,settled,snapshot_required,snapshot_uri,plugin_type?` | `plugin_type` 仅在单插件变化时出现 |
| `device.state.changed` | `state,control_plane_state,detail` | 只有 `state` 的 `offline/connecting/online` 是稳定业务枚举 |
| `face.library.changed` | `reason,affected_count,snapshot_required,snapshot_uri,folders_snapshot_uri,folder_id?` | 不包含人员身份、照片或特征向量 |
| `upgrade.catalog.changed` | `status,message,checked_at,app,firmware,snapshot_required,snapshot_uri` | `app/firmware` 仅含 `supported,current_version,latest_version,has_update,release_id,mandatory` |
| `upgrade.state.changed` | `reason,kind,release_id,phase,progress,active,current_version,target_version,message,started_at,updated_at,finished_at,snapshot_required,snapshot_uri` | 进度详情读取 `upgrades/status` |
| `retrieval.settings.changed`、`retrieval.sync.queued` | `reason,success,enabled,started,applying,sync_queued,indexing,index_size,meta_size,total_sync_rounds,total_sync_failures,last_sync_started_ms,last_sync_finished_ms,last_sync_stage,snapshot_required,snapshot_uri` | 不包含查询文本、模型/数据库路径或检索命中 |
| `retrieval.index.changed` | `reason,success,enabled,index_size,meta_size,total_sync_rounds,total_sync_failures,last_sync_started_ms,last_sync_finished_ms,last_sync_stage,alarm_upsert_ok,alarm_upsert_fail,record_index_ok,record_index_fail,snapshot_required,snapshot_uri` | 表示索引状态可能变化，读取 `retrieval/index/status` |
| `operation.state.changed` | 完整 Operation | 只投递给创建该 Operation 的认证主体 |

事件日志是进程内有界日志，不是永久消息队列，当前最多 1024 条且总计不超过 4 MiB。交付语义为至少一次，同一 `event_id` 可能通过实时推送和断线补偿重复到达；不同生产线程的实时到达次序也不能替代 `sequence`。客户端必须去重并只确认连续处理位置。服务重启或游标早于当前日志窗口时返回 `40904 EVENT_CURSOR_EXPIRED`，客户端应读取业务快照后用新游标重新订阅。

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
- WSS 订阅与创建它的连接绑定，连接关闭后 `subscription_id` 立即失效。重连时传最后确认的 `cursor` 新建订阅，不要传旧 ID。
- `replay_items[]` 一次最多 100 条；建立订阅后继续调用 `events/poll` 直到 `has_more=false`。补偿期间缓存实时事件，再按 `sequence` 合并。
- `expires_in_ms` 是订阅空闲 TTL。WSS 心跳不续该 TTL；在到期前通过 `events/poll` 或同连接重新 `events/subscribe` 刷新活动时间。

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

仅当 `items[]` 已完成业务处理后才能 ACK 对应的连续 `next_cursor`。不能因为后续事件先到达而越过尚未处理的事件推进游标。

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

- 当前 `features[]` 是用于页面降级的高层能力提示，可包含 `tasks`、`task_operations`、`alarms`、`plugin_inventory`、`upgrade_status`、`retrieval_status`，不是全部开放 URI 的枚举。录像、人脸、日志和事件等已发布能力当前可能未单独出现在该数组中。
- 客户端判断接口是否属于本协议时以本文档为准，判断当前凭证是否可调用时以 `session/get.scopes[]` 和实际响应为准；不得因为 `features[]` 未列出某项就删除客户数据，也不得因为列出了某项就绕过 Scope 和接口错误处理。

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
Operation 成功完成后的 `result` 为 TaskRuntime。

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
Operation 成功完成后的 `result` 包含 `task`（新定义）和 `runtime`（TaskRuntime）。

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


#### 8.13.13 设备、任务、节点状态刷新建议

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
Operation 成功完成后的 `result` 包含 `atomic=false`、`total`、`succeeded`、`failed`、`items[]`；每项为 `face_id`、`state`，失败项另含 `code`、`msg`。

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
- 当前版本的同步状态快照可能带有模型、数据库或索引路径类诊断字段。这些字段不是稳定业务契约，客户界面不得展示，平台不得据此拼接下载地址或执行文件操作，普通业务日志也应过滤；后续版本可能对其删除或脱敏。

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

`config_path` 以及 `config/persisted_config` 中的模型、数据库、索引路径属于设备诊断字段，不属于客户可配置的文件接口。客户端不得显示、持久化依赖或回传这些路径；更新配置时只发送本文列出的稳定配置字段。


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
- 响应中的 `config_path` 仅供受控诊断，不得作为后续请求参数、下载地址或客户配置项。

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

升级模块沿用已有字段命名：当前数值型 `checkedAt`、`startedAt`、`updatedAt`、`finishedAt` 以及升级事件 `data` 中对应的 snake_case 字段为 Unix 秒；`publishedAt` 通常是升级清单提供的 ISO 8601 字符串。事件外层 `occurred_at_ms` 仍为 Unix 毫秒。客户端必须按字段定义换算，不能把秒值直接当毫秒显示。

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

当前分页游标不承诺跨设备重启或跨大规模数据变更形成数据库快照。遍历期间资源可能新增或删除，客户端应按资源主键去重；用于持续同步时，应结合实时事件或定期全量校准，不能仅依赖一次长分页扫描。

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

幂等记录按认证主体隔离并保留 24 小时。同一主体在保留期内使用相同 Key 和相同 URI/参数会返回原结果；相同 Key 对应不同请求返回 `40902`。超过 24 小时后不得依赖设备记忆该 Key，客户平台仍应保存自己的业务请求状态以避免重复执行。

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
| WSS 连接 | 单认证主体 4；服务端总连接上限 64 |
| 未消费 WSS Ticket | 单认证主体 8；服务端合计 1024；每张 60 秒且只能使用一次 |
| WSS 消息速率 | 单连接 100 条/秒；超过后关闭连接 |

达到上限返回 `42901`，不会排队占用本地网页或 CloudWS 的控制资源。交付版本可以降低这些上限；客户端不得依赖高频轮询代替事件订阅。

## 10. 错误码

| code | HTTP | 含义 |
|---:|---:|---|
| `0` | 200/202 | 成功或异步受理 |
| `400` | 400 | JSON 解析失败或传输层请求包络非法 |
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

## 13. 当前版本能力边界

本章用于避免客户把“可查询状态”“实时变化通知”和“完整数据下载”混为一谈。项目验收以本表、交付 Scope 和接口实际响应为准；`device/capabilities.features[]` 当前只是高层提示，不是完整 API 清单。

| 功能 | 当前支持情况 | 对接方式或限制 |
|---|---|---|
| 能力发现 | 部分结构化支持 | `device/capabilities` 返回运行位置、节点类型和高层 Feature；全部 URI、Topic、Scope 与限制以本协议为准 |
| 任务创建、编辑、复制、删除、启停 | 已支持 | HTTPS/WSS Command；写操作使用幂等键，修改使用 Revision |
| 任务运行状态 | 已支持 | `task.state.changed` 通知，`tasks/runtime` 读取权威状态 |
| 节点状态 | 部分支持 | `node.state.changed` + `tasks/node_metrics`；当前为任务运行态聚合，不含真实逐插件 FPS、延迟、丢帧和资源占用 |
| 设备身份、版本、授权 | 已支持 | `device/get` |
| CPU/NPU/内存/CMM/温度/存储 | 已支持按需读取 | `device/metrics`；当前不周期推送，客户监控页按第 6.6 节节流采集 |
| 客户平台判断设备在线 | 已支持 | 以客户自身 WSS、`ping/pong` 和超时策略判断；不要使用 AIBox 官方云控制面状态代替 |
| AIBox 云控制面连接状态 | 已支持变化通知 | `device.state.changed`；仅表示设备到 AIBox 云控制面 |
| 网络状态 | 只读 | `device/network/get`；本版本不开放修改 IP、网关、DNS 或重启网卡 |
| 实时视频/任务媒体输出 | 已支持输出发现 | `media/outputs` 返回任务已注册的 RTSP 等输出；本版本不开放本地网页专用 WebRTC 信令会话，客户平台需自行播放 RTSP 或通过 Gateway 转码 |
| 实时告警通知 | 已支持快照型告警 | `alarm.created/alarm.deleted`；收到后读取 `alarms/get`。当前事件生产路径在告警图片落库时触发 |
| 告警历史 | 已支持 | `alarms/list/get/delete` |
| 告警原图下载 | 本版本未作为通用 OpenAPI 发布 | 不返回内部文件路径；需要安全图片 Ticket 的项目应在交付能力清单中单独确认 |
| 录像实时通知与历史 | 已支持 | `record.created/deleted`、`records/list/get` |
| 录像下载 | 已支持 | `records/download_ticket`；使用短期 HMAC Ticket 和 HTTP Range |
| 设备日志 | 只读查询 | `logs/list/get`；当前无日志实时流，禁止通过高频轮询模拟日志推送 |
| 人脸库元数据和写入 | 已支持 | `faces/*`；写操作异步执行并返回 Operation |
| 人脸照片读取 | 本版本不返回照片内容 | `photo_available` 仅表示是否存在；不返回设备路径、图片二进制或特征向量 |
| 插件/节点目录读取 | 已支持 | `nodes/catalog/schema`、`plugins/list/health` |
| 第三方执行插件安装、启停和卸载 | 本版本未开放 | `plugin.catalog.changed` 可通知由本地网页或云端管理面产生的目录变化 |
| 智能检索 | 按设备能力启用 | `retrieval/*`；查询结果只返回给发起请求的主体，不广播到事件通道 |
| 在线升级 | 按设备升级目录执行 | `upgrades/check/status/apply/cancel`；不接受第三方任意文件直接覆盖系统 |
| 设备重启、关机和系统时间设置 | 本版本未开放 | 不得调用本地网页私有 URI 或通过升级接口变相执行系统命令 |
| 存储挂载、格式化和录像策略配置 | 本版本未开放管理接口 | `device/metrics.storage[]` 只读；录像查询、下载和删除按 `records/*` 执行 |
| 本地用户、云绑定和许可证写入 | 本版本未开放 | OpenAPI Client、本地网页账号、CloudWS 设备身份相互隔离；`device/get.authorization` 仅供读取 |
| Webhook、MQTT | 设备直连分册未提供 | 实时通道为 WSS，HTTPS `events/poll` 用于断线补偿；Gateway 模式可另行提供北向推送协议 |
| 多设备、跨租户集中管理 | 不属于设备直连接口 | 应通过 Device Gateway；设备本地 OpenAPI Client 密钥不能直接扩展为云端多设备权限 |

交付方不得用本地网页私有 URI、设备内部路径或未列入本协议的临时字段填补上述边界。客户需要新增能力时，应以新增 OpenAPI URI/Topic、Scope、错误码和版本说明的方式正式扩展。

## 14. 客户对接验收清单

交付验收至少覆盖以下项目；“通过”必须同时保留请求 `request_id`、HTTP 状态、业务 `code` 和必要的设备日志，密钥与 Token 必须脱敏：

| 验收项 | 通过标准 |
|---|---|
| TLS 与凭证隔离 | HTTPS/WSS 使用可信证书；第三方密钥不能登录本地网页、不能充当 CloudWS 身份 |
| Token 生命周期 | 能获取 Token、提前续期；密钥轮换/禁用后旧 Token 请求返回 401/403，客户停止旧 WSS |
| HTTPS/WSS 基础调用 | `session/get`、`device/get` 在两种通道返回一致业务语义；WSS `id` 能正确关联并发响应 |
| 事件可靠性 | 能订阅全部获授权 Topic；覆盖重复事件、超过 100 条补偿、乱序合并、30 分钟续订、断线重连和 `40904` 全量恢复 |
| 任务闭环 | 完成节点目录加载、任务创建/读取/修改/复制/删除、Revision 冲突、启停重启和 Operation 终态 |
| 状态与告警 | 能处理任务/节点/设备状态事件；实时告警落库后读取 `alarms/get`，并正确处理删除事件 |
| 媒体与录像 | 能获取任务输出；录像 Ticket 过期受控、支持 Range 下载，删除后客户缓存同步 |
| 可选业务域 | 按已授予 Scope 验证人脸、检索和升级；未授予 Scope 必须稳定返回 `40301` |
| 重试与过载 | 相同幂等键不重复执行；不同参数复用 Key 返回 `40902`；429 按 `Retry-After` 退避且不影响本地网页和 CloudWS |
| 重启恢复 | 设备/服务重启后客户可重新鉴权、重建快照和事件订阅，不依赖旧 Ticket、旧订阅 ID 或过期游标 |
