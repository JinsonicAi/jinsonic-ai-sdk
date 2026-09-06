# AIBox Third-Party OpenAPI Integration Protocol

| Document property | Value |
|---|---|
| Document title | AIBox Third-Party OpenAPI Integration Protocol |
| Document version | `1.4.9` |
| Protocol version | `OpenAPI v1` |
| Release date | `2026-09-06` |
| Intended audience | Third-party platforms and customer-developed web/server applications |
| Chinese version | [AIBOX_OPENAPI_INTEGRATION_PROTOCOL_ZH.md](../../zh/reference/openapi-protocol.md) |

## Revision History

| Version | Date | Summary |
|---|---|---|
| `1.4.9` | `2026-09-06` | Document bounded WSS error handling/retries and legacy certificate compatibility; business APIs unchanged |
| `1.4.8` | `2026-09-06` | Add only self-service Client provisioning and password-change invalidation; existing Client Credentials/business APIs unchanged |
| `1.4.6` | `2026-09-06` | Added runnable Python/Node.js/curl customer examples; corrected the input field to rtsp_url; preserved the graph in the task-update example; documented validation and delivery boundaries. No business protocol changes. |
| `1.4.5` | `2026-08-03` | Completed all 18 realtime event contracts, token/WSS renewal, event recovery and subscription renewal, capability boundaries, idempotency/operation retention, and the customer acceptance checklist. |

> **Download the examples:** [Python / Node.js / curl example package 1.4.9](../../assets/downloads/aibox-openapi-examples-1.4.9.zip) ([SHA-256 checksum](../../assets/downloads/aibox-openapi-examples-1.4.9.zip.sha256)). The included `TaskManager/examples/openapi/README.md` provides step-by-step instructions for obtaining credentials, connecting, creating/updating/starting/stopping tasks, processing events and downloading recordings.

> **Public boundary:** This document only defines how a customer connects to a device, sends requests, and processes responses. AIBox internal web APIs, databases, and plugin files are not part of the public contract.

> **Transport boundary:** This is the device-direct edition. It applies when the customer system can reach an AIBox through a LAN, private network, VPN, or private APN. If the device is behind NAT, a mobile network, or the public Internet, the device must initiate a persistent WSS connection to the AIBox Device Gateway. The customer platform then calls the Gateway northbound API; port `8099` on the device must not be exposed directly to the Internet. The device uplink protocol and Gateway API share the business URIs and data models defined here, but use separate device identities, tenant authorization, and connection sessions.

## Document Guide

| Goal | Section |
|---|---|
| Complete the first connection and authentication | Sections 1–2 |
| Understand HTTP/WSS envelopes | Sections 3–4 |
| Create, edit, start, and stop tasks | Section 5 |
| Receive realtime alarms and task/node/device state | Section 6 |
| Obtain media outputs, recordings, and alarm history | Section 7 and Section 8.14 |
| Look up every API | Section 8 |
| Handle pagination, retries, and errors | Sections 9–10 |
| Apply security and compatibility requirements | Sections 11–12 |
| Confirm current capability boundaries | Section 13 |
| Execute customer acceptance tests | Section 14 |

## 1. Connection Information

| Item | Value |
|---|---|
| HTTPS endpoint | `https://<device-host>:8099/openapi/v1/command` |
| WSS endpoint | `wss://<device-host>:8099/openapi/v1/ws?ticket=<ticket>` |
| Payload | UTF-8 JSON |
| HTTP method | `POST` |
| Authentication header | `X-Access-Token: <access_token>` |
| Maximum HTTP/WSS message | `4 MiB` |
| Default access-token lifetime | `3600` seconds |

All HTTPS business calls are sent to `/openapi/v1/command`. Every other `/openapi/v1/...` value in this document is the `uri` inside the JSON request (a **Command URI**), not a directly accessible HTTP path. Historical internal compatibility paths are not part of this protocol.

```http
POST /openapi/v1/command HTTP/1.1
Content-Type: application/json
X-Access-Token: <access_token>

{"uri":"/openapi/v1/device/get","param":{}}
```

### 1.1 Deployment Models

| Model | Connection direction | Production channel | Typical use |
|---|---|---|---|
| Device direct | Customer system → AIBox | HTTPS + WSS | LAN, VPN, private network, private APN |
| Customer cloud management | AIBox → Device Gateway; customer platform → Gateway | Device WSS uplink; Gateway HTTPS/WSS/Webhook northbound | NAT, 4G/5G, public Internet, centralized multi-device management |

A customer-developed browser must not store `client_secret`. The browser should authenticate to the customer's own BFF/Gateway; that trusted server holds the third-party credentials. The current Client Credentials flow is for confidential server-side clients, not public browser clients.

Device-direct HTTPS does not promise arbitrary browser CORS access. A customer web application should call its own BFF, which calls the device OpenAPI. If a browser must establish a device WSS directly, its `Origin` must be same-origin with the device or included in an exact `scheme://host:port` allowlist configured by the delivery team. `*` is prohibited. The certificate must be trusted by the browser and cover the actual hostname or IP used to connect.

Each third-party system uses separate credentials:

```text
client_id     Public identifier, for example aibc_xxx
client_secret Confidential secret, for example aibsk_xxx; displayed once
```

**Customers can obtain the pair using the device web-login username and password.** Send the new `/openapi/v1/clients/bootstrap` command to HTTPS `POST /openapi/v1/command` and read `result.client_id` and `result.client_secret`. The existing `auth/token` exchange and business APIs are unchanged; there is no separate account-token mode. Administrator `clients/create` remains available; `clients/get/list` never returns an original secret.

#### 1.1.1 Self-service credentials (first use or after a password change)

API index: `POST /openapi/v1/clients/bootstrap`. This is a Command URI; the actual HTTP endpoint remains `/openapi/v1/command`.

Do not send `X-Access-Token`:

```json
{"uri":"/openapi/v1/clients/bootstrap","request_id":"bootstrap-unique-001","param":{"username":"<web account>","password":"<actual password>","display_name":"customer-platform","scopes":["device.read","task.read"]}}
```

Success has `code=0` and Client metadata plus `client_id`, `client_secret`, and `secret_returned_once:true`; it does not issue an access token. `display_name` is optional. Omitted `scopes` uses the account's allowed business scopes; explicit least privilege is recommended. Scope escalation and credential-management authority are not permitted. Runnable Python/Node.js/curl provisioning programs and instructions are in the README included in the [example package](../../assets/downloads/aibox-openapi-examples-1.4.9.zip).

A committed web-password change, account deletion/recreation, or scope change invalidates all self-service credentials derived from that account and their tokens. **Changing back to the old password does not reactivate them.** Obtain a new pair with the current password. Subsequent WSS messages, heartbeats and event delivery reject the old identity; reconnect with a new Token/Ticket and resume the persisted cursor. Already accepted operations are not automatically canceled. Independently administrator-provisioned clients are unaffected by unrelated account changes.

The same account and `request_id` issues a secret once only; duplicates return `40901 / CLIENT_SECRET_ALREADY_ISSUED`, never the secret. Reconcile a lost response by request ID rather than creating clients in a retry loop. Limits: 16 enabled self-service clients per current account generation and 4096 self-service records per device; exceeding either returns `40901 / ACCOUNT_CLIENT_LIMIT`. Invalid password: `40101`; scope denied: `40301`; rate limit: `42901`; temporarily unreadable store: `50301` (fail closed, without permanent revocation). Credentials belong only in the HTTPS POST JSON body, never URLs or logs.

`client_secret` is used only to obtain a short-lived access token. It must not appear in a URL, browser bundle, or ordinary log.

An `openapi_client` access token is accepted only in `X-Access-Token`. URL query parameters, cookies, top-level JSON `token`, and `param.access_token` are not OpenAPI authentication mechanisms. Compatibility token parsing used by the built-in web UI is not a public contract.

### 1.2 Scope Matrix

Provision the minimum required scopes; do not grant all scopes by default.

| Scope | Authorized capability |
|---|---|
| `device.read` | Read device identity, authorization, versions, resources, network state, and runtime locations; subscribe to device-state events |
| `task.read` | Read tasks, nodes, node catalog, and plugin state; subscribe to task/node/plugin events |
| `task.write` | Create, save, clone, delete, and validate tasks |
| `task.execute` | Start, stop, and restart tasks |
| `media.read` | Read media outputs and recording metadata; download recordings; subscribe to recording events |
| `media.manage` | Delete recordings; normally granted together with `media.read` |
| `alarm.read` | Read alarms and subscribe to realtime alarm events |
| `alarm.manage` | Delete alarms; normally granted together with `alarm.read` |
| `log.read` | Read device logs |
| `face.read` | Read face groups and person metadata; subscribe to face-library events |
| `face.write` | Register, update, delete, and batch-import faces |
| `retrieval.use` | Read and configure retrieval, synchronize indexes, run searches, and subscribe to retrieval events |
| `upgrade.manage` | Check, start, and cancel upgrades; subscribe to upgrade events |
| `openapi.clients.manage` | Local delivery administrator credential management; never grant to a third-party Client |

Scopes do not imply one another. For example, `task.write` does not imply `task.read`, and `alarm.manage` does not imply `alarm.read`. Grant both explicitly when the client needs both read and write access.

### 1.3 Token Lifecycle and Device Time

This protocol does not issue refresh tokens. Cache the access token on the customer server and request a new one five minutes before `expires_at`, or when less than 10% of its lifetime remains, whichever occurs first. Do not request a token for every business call. By default, one Client can hold at most 16 active tokens.

A public WSS connection is bound to the access token that issued its Ticket. Renew a long-lived connection in this order: obtain a new token → obtain a new Ticket → establish a new WSS → create a subscription using the persisted event `cursor` and finish replay → close the old WSS. After `40101`, secret rotation, scope changes, or Client disablement, stop using the old connection and token. Never treat an old WSS as authorized merely because the TCP/WebSocket connection has not closed yet.

`server_time_ms`, fields ending in `_ms`, and event `occurred_at_ms` are Unix milliseconds. Both device and customer platform must use reliable NTP/time synchronization. A client may compare `server_time_ms` to detect clock skew, but must not overwrite device business timestamps with local receive time. Section 8.17 documents the few legacy upgrade fields that do not use `_ms`.

## 2. First Call in Five Minutes

### 2.1 Obtain an Access Token

Do not include `X-Access-Token` in this request.

```bash
curl -X POST "https://<device-host>:8099/openapi/v1/command" \
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

Successful response:

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

Authentication failure:

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

Repeated failures temporarily lock the Client and return HTTP `429`. Wait for `retry_after_ms` before retrying.

### 2.2 Validate the Session

```bash
curl -X POST "https://<device-host>:8099/openapi/v1/command" \
  -H "Content-Type: application/json" \
  -H "X-Access-Token: <access_token>" \
  -d '{"uri":"/openapi/v1/session/get","param":{}}'
```

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

### 2.3 Read the Device and Tasks

Recommended initialization order:

```text
/openapi/v1/device/get
/openapi/v1/device/capabilities
/openapi/v1/nodes/catalog
/openapi/v1/tasks/list
```

```json
{
  "uri": "/openapi/v1/tasks/list",
  "request_id": "req-task-list-001",
  "param": {"page_size": 50}
}
```

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

## 3. HTTP Request and Response Envelopes

### 3.1 Request

```json
{
  "uri": "/openapi/v1/tasks/get",
  "request_id": "req-unique-001",
  "idempotency_key": "write-request-unique-key",
  "param": {"task_id": "task-001"}
}
```

| Field | Required | Description |
|---|---|---|
| `uri` | Yes | Command URI |
| `param` | Yes | Business parameters; send `{}` when empty |
| `request_id` | Recommended | Client-generated trace ID; use 1–128 printable ASCII characters and keep it unique per call |
| `idempotency_key` | Required for mutation APIs | Preserve for retries of the same business request; 1–128 printable ASCII characters |

### 3.2 Successful Response

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

### 3.3 Error Response

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

Check both HTTP Status and JSON `code`; only `code=0` is a business success. HTTPS responses include `X-Request-ID`. OpenAPI responses use `Cache-Control: no-store` and must not be cached by browsers or proxies.

The server generates a trace value when `request_id` is absent, but customer platforms must not depend on this for end-to-end tracing. Preserve the original `request_id` and `idempotency_key` when retrying the same business call. Generate a new `request_id` for a new business call.

## 4. WSS Connection and Calls

WSS supports low-latency requests and realtime events. Authentication, WSS Ticket issuance, and recording downloads remain HTTPS-only.

### 4.1 Request a One-Time Ticket

Send this command over HTTPS with `X-Access-Token`:

```json
{
  "uri": "/openapi/v1/ws/ticket",
  "request_id": "req-ws-ticket-001",
  "param": {}
}
```

```json
{
  "code": 0,
  "msg": "OK",
  "result": {
    "ticket": "<64-hex-character-ticket>",
    "expires_at_ms": 1785600060000,
    "websocket_path": "/openapi/v1/ws",
    "protocol": "aibox.openapi.v1",
    "max_message_bytes": 4194304,
    "heartbeat_interval_ms": 25000
  }
}
```

The Ticket expires after 60 seconds and is single-use. Obtain a new Ticket for every connection attempt.

### 4.2 Establish the Connection

```text
wss://<device-host>:8099/openapi/v1/ws?ticket=<ticket>
```

No `Sec-WebSocket-Protocol` value is currently required. `protocol=aibox.openapi.v1` is the application-envelope version. Send UTF-8 JSON text only. The server does not negotiate `permessage-deflate`; message size is calculated from uncompressed JSON bytes.

The server sends `ready` after successful authentication:

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

### 4.3 Send Requests

```json
{
  "type": "request",
  "id": "ws-call-001",
  "uri": "/openapi/v1/tasks/list",
  "request_id": "req-task-list-001",
  "param": {"page_size": 50}
}
```

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

`id` correlates concurrent calls on one WSS. A WSS message cannot carry or switch the authenticated token.

Application heartbeat:

```json
{"type":"ping"}
```

```json
{"type":"pong","server_time_ms":1785600000000}
```

Send `ping` according to `ready.heartbeat_interval_ms`. If no `pong` or other valid response arrives for two consecutive intervals, close the connection and obtain a new Ticket. Do not retry forever on a stale connection.

| Condition | Server behavior | Client action |
|---|---|---|
| Invalid, expired, reused Ticket, or connection limit | Close code `1008` | Obtain a new token/Ticket; close unnecessary old connections |
| Binary message | Close code `1003` | Send UTF-8 JSON text |
| Message larger than 4 MiB | Close code `1009` | Reduce the request; do not transfer large files over WSS |
| More than 100 messages/second | Close code `1008` | Reduce concurrency and reconnect with exponential backoff |
| Slow consumer or send-buffer limit | Close code `1013` | Recover with `events/poll`, then reduce subscriptions or improve consumption |

One authenticated subject may hold at most four public WSS connections and eight unconsumed Tickets. Browser tabs or service instances must share connections or explicitly manage this quota.

## 5. Task Integration Flow

### 5.1 Discover Available Nodes

Call these APIs first:

```text
/openapi/v1/nodes/catalog
/openapi/v1/nodes/schema
```

```json
{
  "uri": "/openapi/v1/nodes/schema",
  "param": {"node_type": "netclient"}
}
```

The returned `schema` describes what the current device software actually supports. Do not hard-code the plugin count or infer node parameters from the hardware model.

### 5.2 Create a Task

```json
{
  "uri": "/openapi/v1/tasks/create",
  "request_id": "req-task-create-001",
  "idempotency_key": "task-create-task-001",
  "param": {
    "task": {
      "task_id": "task-001",
      "task_name": "Entrance Detection",
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

The successful response contains the complete task with `revision=1`:

```json
{
  "code": 0,
  "msg": "OK",
  "result": {
    "task_id": "task-001",
    "task_name": "Entrance Detection",
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

### 5.3 Update a Task

Read the latest task and `revision` with `tasks/get`, modify the complete object, and submit it:

```json
{
  "uri": "/openapi/v1/tasks/save",
  "request_id": "req-task-save-001",
  "idempotency_key": "task-save-task-001-r1",
  "param": {
    "expected_revision": 1,
    "task": {
      "task_id": "task-001",
      "task_name": "Entrance Detection - Updated",
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

`tasks/save` replaces the complete task; it is not a partial Patch. This example preserves the node from Section 5.2. In real integrations, copy all nodes, edges and configurations returned by `tasks/get` before editing; empty arrays do not mean "unchanged". A revision conflict returns:

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

After a conflict, read the task again and let the customer application decide how to merge. Never overwrite blindly.

### 5.4 Start a Task

```json
{
  "uri": "/openapi/v1/tasks/start",
  "request_id": "req-task-start-001",
  "idempotency_key": "task-start-task-001-r2",
  "param": {"task_id": "task-001", "revision": 2}
}
```

Asynchronous acceptance returns HTTP `202`:

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

Poll the Operation:

```json
{
  "uri": "/openapi/v1/operations/get",
  "param": {"operation_id": "op_xxx"}
}
```

Only `state=succeeded` means the action succeeded. Terminal states are:

```text
succeeded | failed | canceled | interrupted
```

Finally, call `/openapi/v1/tasks/runtime` to verify the actual runtime state.

### 5.5 Stop and Delete a Task

Stop:

```json
{
  "uri": "/openapi/v1/tasks/stop",
  "idempotency_key": "task-stop-task-001",
  "param": {"task_id": "task-001"}
}
```

Delete:

```json
{
  "uri": "/openapi/v1/tasks/delete",
  "idempotency_key": "task-delete-task-001-r2",
  "param": {"task_id": "task-001", "expected_revision": 2}
}
```

## 6. Realtime Events

### 6.1 Realtime Reporting Capabilities

After establishing WSS, a third-party platform can subscribe to realtime alarms and task/node/device state changes. Events are notifications; authoritative business data is always read from the listed snapshot API.

| Business data | Event | Authoritative read | Current semantics |
|---|---|---|---|
| New alarm | `alarm.created` | `alarms/get` | Sent after the alarm record and snapshot have been stored |
| Alarm deletion | `alarm.deleted` | Remove from customer cache | Sent after deletion |
| Task runtime state | `task.state.changed` | `tasks/runtime` | Start, stop, restart, failure, and other state changes |
| Node state | `node.state.changed` | `tasks/node_metrics` | The node-state snapshot for a task may have changed |
| Device control-plane state | `device.state.changed` | `device/get` | Device-to-AIBox-cloud state: `offline/connecting/online` |
| Device resource metrics | No periodic event | `device/metrics` | CPU, NPU, memory, temperature, and storage are sampled on demand |

> **Online-state boundary:** In device-direct mode, the customer platform determines whether the device is online from its own WSS connection and timely `ping/pong`. `device.state.changed` describes the device's connection to the AIBox cloud control plane; it is not a replacement for the customer's own connection state.

> **Node-metric boundary:** `node.state.changed` is a snapshot-invalidation notification, not periodic per-node telemetry. `tasks/node_metrics` currently maps aggregate task runtime state to nodes. Real per-plugin FPS, processing latency, and dropped-frame metrics are not public; the server returns `metrics_available=false`. Do not treat this as a node failure.

### 6.2 Supported Topics

| Topic | Required scope | `resource.type` | Purpose |
|---|---|---|---|
| `alarm.created` | `alarm.read` | `alarm` | Alarm created |
| `alarm.deleted` | `alarm.read` | `alarm` | Alarm deleted |
| `record.created` | `media.read` | `record` | Recording created |
| `record.deleted` | `media.read` | `record` | Recording deleted |
| `task.created` | `task.read` | `task` | Task definition created |
| `task.updated` | `task.read` | `task` | Task definition updated |
| `task.deleted` | `task.read` | `task` | Task definition deleted |
| `task.state.changed` | `task.read` | `task` | Task runtime state changed |
| `node.state.changed` | `task.read` | `task_nodes` | Node snapshot for a task may have changed |
| `plugin.catalog.changed` | `task.read` | `plugin_catalog` | Plugin catalog changed |
| `device.state.changed` | `device.read` | `device` | Cloud control-channel state changed |
| `face.library.changed` | `face.read` | `face_library` | Face or face-group data changed |
| `upgrade.catalog.changed` | `upgrade.manage` | `upgrade_catalog` | Upgrade candidates changed |
| `upgrade.state.changed` | `upgrade.manage` | `upgrade` | Upgrade phase or terminal state changed |
| `retrieval.settings.changed` | `retrieval.use` | `retrieval_index` | Retrieval settings were saved or are being applied |
| `retrieval.sync.queued` | `retrieval.use` | `retrieval_index` | Manual index synchronization entered the bounded queue |
| `retrieval.index.changed` | `retrieval.use` | `retrieval_index` | Index rebuild, synchronization, or settings application completed |
| `operation.state.changed` | Authenticated | `operation` | State of an asynchronous operation owned by this subject |

Events are filtered by authenticated subject and Scope. `operation.state.changed` is private: ordinary clients, other clients, and wildcard administrators do not receive Operations they did not submit.

Task-definition, node, and plugin events use a lightweight-event-plus-snapshot model to avoid sending large configuration objects over WSS.

| Topic | Authoritative read after receipt |
|---|---|
| `task.created`, `task.updated` | `tasks/get` |
| `task.deleted` | Remove `resource.id` from the customer cache |
| `task.state.changed` | `tasks/runtime` |
| `node.state.changed` | `tasks/node_metrics` |
| `plugin.catalog.changed` | `nodes/catalog` or `plugins/list` |
| `device.state.changed` | `device/get`, `device/metrics` |
| `face.library.changed` | `faces/folders`, `faces/list` |
| `upgrade.catalog.changed` | `upgrades/check` |
| `upgrade.state.changed` | `upgrades/status` |
| `retrieval.settings.changed` | `retrieval/settings/get` |
| `retrieval.sync.queued`, `retrieval.index.changed` | `retrieval/index/status` |

Face events contain only group identifiers, affected counts, and snapshot URIs; they never contain names, phone numbers, identity numbers, photos, or feature vectors. Upgrade events do not include download URLs, local paths, or verification secrets. Retrieval events do not include query text, internal database/model paths, or search hits.

### 6.3 Subscribe to Alarms and Node/Device State

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

WSS subscriptions return `delivery=realtime+poll`; HTTPS subscriptions return `delivery=poll`. This example requires `alarm.read`, `task.read`, and `device.read`. If any required Scope is missing, the whole request returns HTTP 403; the server does not create a partial subscription.

### 6.4 Realtime Alarm Event

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

Process it in this order:

1. Use `resource.id` as `alarm_id` in `alarms/get` and read authoritative fields.
2. Deduplicate by `event_id` and persist the customer business record.
3. Call `events/ack` only after the business transaction is committed.
4. For `alarm.deleted`, remove `resource.id` from the customer cache; a later `alarms/get` may return 404.

The event contains neither image bytes nor a device file path. The public Alarm model only declares `image_available`. Projects that require original alarm-image download must explicitly confirm a secure image-Ticket API in their delivery capability list. Do not construct internal file paths or depend on built-in web static URLs.

### 6.5 Node-State Event

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

Call `tasks/node_metrics` with `{"task_id":"task-001"}` and refresh all nodes for that task in one request. Do not poll every node independently, and do not treat the event `state` as the final state of every node.

### 6.6 Device State and Resource Metrics

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

Stable `state` values are `offline`, `connecting`, and `online`. `control_plane_state` and `detail` are diagnostic and their free-text values are not a stable enum. CPU, NPU, memory, CMM, temperature, and storage are not included in this event. A visible monitoring page should call `device/metrics` every 3–5 seconds with 10%–20% jitter, and pause or slow down while hidden/backgrounded.

### 6.7 Reliable Delivery and Disconnect Recovery

Events are delivered at least once. A client must:

1. Deduplicate by `event_id`.
2. Maintain a contiguous processing position by `sequence`; concurrent producers can result in different arrival order. Never ACK past an unprocessed event.
3. ACK after committing business data and persist the last contiguously processed `cursor`.
4. `events/subscribe` returns at most 100 `replay_items[]`. Continue with `events/poll` until `has_more=false`. Buffer new realtime events while replaying, then merge by `sequence`.
5. After WSS disconnect, create a new subscription with the same Topics and last successful `cursor`, finish replay, then resume realtime consumption.

WSS connectivity does not guarantee that state is current. On first connection, reconnect, `40904`, or `snapshot_required=true`, rebuild from the authoritative snapshot APIs.

Closing WSS immediately invalidates every `subscription_id` bound to it. An old ID cannot be resumed on another connection or used directly with HTTPS `events/poll`. The cross-connection recovery identity is the persisted `cursor`. For HTTPS compensation after WSS closes, first create a new `delivery=poll` subscription with `topics + cursor`, then poll the new `subscription_id`.

An idle subscription expires after 30 minutes. WSS `ping/pong` only keeps the connection alive; it does not refresh subscription TTL. Even when Topics are quiet, call `events/poll` at least every 20 minutes, or reissue `events/subscribe` on the same WSS with the same `subscription_id`, Topics, and last successful `cursor`. `40401` means the subscription no longer exists; create a new one with the persisted cursor.

The event journal retains at most 1024 events and 4 MiB total, evicting the oldest entry when either limit is reached. There is no time-based retention guarantee. A client offline for a long period must handle `40904` by rebuilding authoritative snapshots.

### 6.8 Customer Platform Startup and Recovery Order

1. Obtain an access token over HTTPS and verify Scopes with `session/get`.
2. Obtain a one-time WSS Ticket, connect, and wait for `ready`.
3. Subscribe immediately. Process `replay_items[]` before realtime events.
4. In parallel, read `device/get`, `device/metrics`, and `tasks/list`; read `tasks/runtime` and `tasks/node_metrics` for watched tasks.
5. Initialize alarms with `alarms/list`, then maintain them with `alarm.created/alarm.deleted`.
6. Deduplicate by `event_id`; process contiguously by `sequence`; commit, ACK, and persist `next_cursor`. Poll until `has_more=false`, then merge buffered realtime events.
7. Refresh subscription activity within 20 minutes. Reconnect with exponential backoff and rebuild the subscription from the saved cursor. For HTTPS compensation, create a new poll subscription. On `40904`, discard the cursor, rebuild all authoritative snapshots, and establish a new subscription.

Do not read snapshots first and subscribe several seconds later; that creates a window in which state changes can be lost.

## 7. Media, Recordings, and Alarms

### 7.1 Get Task Output URLs

```json
{
  "uri": "/openapi/v1/media/outputs",
  "param": {"task_id": "task-001"}
}
```

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

### 7.2 Download a Recording

First list recordings:

```json
{
  "uri": "/openapi/v1/records/list",
  "param": {"task_id": "task-001", "page_size": 20}
}
```

Then request a download Ticket:

```json
{
  "uri": "/openapi/v1/records/download_ticket",
  "param": {"record_id": "rec_xxx"}
}
```

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

Download from the same device HTTPS origin:

```bash
curl -L -H "Range: bytes=0-1048575" \
  "https://<device-host>:8099/record/download?ticket=v1.xxx" \
  -o record.part
```

The Ticket is valid for at most five minutes. Request a new one after expiry; never cache it long-term.

## 8. API Reference

The current implementation exposes `65` Command URIs. Sections 8.1–8.7 are quick indexes; Sections 8.8–8.17 define the shared models and every individual API.

In the index tables, `param` means request parameters, `result` lists the principal success fields, and `?` means optional. In each API card, `POST /openapi/v1/...` is shorthand for the Command URI. The actual HTTPS path is always `/openapi/v1/command`; WSS places the same URI in a `type=request` message.

### 8.1 Session, Operation, and Event APIs

| URI | Scope | param | result |
|---|---|---|---|
| `/openapi/v1/auth/token` | None | `client_id`, `client_secret` | `access_token`, `expires_in`, `scopes` |
| `/openapi/v1/session/get` | Authenticated | `{}` | `subject`, `client_id`, `scopes`, `expires_at` |
| `/openapi/v1/ws/ticket` | Authenticated | `{}` | `ticket`, `expires_at_ms`, `websocket_path` |
| `/openapi/v1/operations/get` | Authenticated | `operation_id` | Operation |
| `/openapi/v1/operations/list` | Authenticated | `page_size?` | `items`, `count` |
| `/openapi/v1/operations/cancel` | Authenticated | `operation_id` | Operation |
| `/openapi/v1/events/subscribe` | Per Topic | `topics[]`, `cursor?`, `subscription_id?` | `subscription_id`, `replay_items`, `next_cursor` |
| `/openapi/v1/events/poll` | Per Topic | `subscription_id`, `cursor?`, `page_size?` | `items`, `next_cursor`, `has_more` |
| `/openapi/v1/events/ack` | Per Topic | `subscription_id`, `cursor` | `acknowledged_cursor` |
| `/openapi/v1/events/unsubscribe` | Per Topic | `subscription_id` | `deleted` |

### 8.2 Device, Node, and Plugin APIs

| URI | Scope | param | result |
|---|---|---|---|
| `/openapi/v1/device/get` | `device.read` | `{}` | Device ID, versions, authorization |
| `/openapi/v1/device/metrics` | `device.read` | `{}` | `compute[]`, `storage[]`, `collected_at_ms` |
| `/openapi/v1/device/runtime_locations` | `device.read` | `{}` | Available runtime locations |
| `/openapi/v1/device/capabilities` | `device.read` | `{}` | `runtime_locations`, `node_types`, `features` |
| `/openapi/v1/device/network/get` | `device.read` | `{}` | Network-interface snapshot |
| `/openapi/v1/nodes/catalog` | `task.read` | `{}` | Component groups and loading progress |
| `/openapi/v1/nodes/schema` | `task.read` | `node_type` | Node Schema |
| `/openapi/v1/graphs/validate` | `task.write` | `task` | `valid`, `warnings` |
| `/openapi/v1/plugins/list` | `task.read` | `{}` | Plugin inventory |
| `/openapi/v1/plugins/health` | `task.read` | `{}` | Plugin health |

### 8.3 Task APIs

| URI | Scope | param | result |
|---|---|---|---|
| `/openapi/v1/tasks/list` | `task.read` | `cursor?`, `page_size?`, `name?` | `items`, `total`, `next_cursor` |
| `/openapi/v1/tasks/get` | `task.read` | `task_id` | Task |
| `/openapi/v1/tasks/create` | `task.write` | `task` | Task |
| `/openapi/v1/tasks/save` | `task.write` | `task`, `expected_revision` | Task |
| `/openapi/v1/tasks/clone` | `task.write` | `source_task_id`, `new_task_id`, `new_task_name?` | Task |
| `/openapi/v1/tasks/delete` | `task.write` | `task_id`, `expected_revision` | `deleted` |
| `/openapi/v1/tasks/start` | `task.execute` | `task_id`, `revision` | Operation, HTTP 202 |
| `/openapi/v1/tasks/stop` | `task.execute` | `task_id` | Operation, HTTP 202 |
| `/openapi/v1/tasks/restart` | `task.execute` | `task_id`, `revision` | Operation, HTTP 202 |
| `/openapi/v1/tasks/apply_and_start` | `task.write task.execute` | `task`, `expected_revision` | Operation, HTTP 202 |
| `/openapi/v1/tasks/runtime` | `task.read` | `task_id` | TaskRuntime |
| `/openapi/v1/tasks/node_metrics` | `task.read` | `task_id` | `nodes[]`, `collected_at_ms` |

### 8.4 Media, Recording, Alarm, and Log APIs

| URI | Scope | param | result |
|---|---|---|---|
| `/openapi/v1/media/outputs` | `media.read` | `task_id` | `items[]` |
| `/openapi/v1/records/list` | `media.read` | `cursor?`, `page_size?`, `task_id?`, `year?`, `month?`, `day?` | `items`, `total`, `next_cursor` |
| `/openapi/v1/records/get` | `media.read` | `record_id` | Record |
| `/openapi/v1/records/download_ticket` | `media.read` | `record_id` | `url`, `expires_at_ms`, `range_supported` |
| `/openapi/v1/records/delete` | `media.manage` | `record_id` | `deleted` |
| `/openapi/v1/alarms/list` | `alarm.read` | `cursor?`, `page_size?`, `task_name?` | `items`, `total`, `next_cursor` |
| `/openapi/v1/alarms/get` | `alarm.read` | `alarm_id` | Alarm |
| `/openapi/v1/alarms/delete` | `alarm.manage` | `alarm_id` | `deleted` |
| `/openapi/v1/logs/list` | `log.read` | `cursor?`, `page_size?`, `source?`, `level?`, `keyword?`, `start_time?`, `end_time?` | `items`, `total`, `next_cursor` |
| `/openapi/v1/logs/get` | `log.read` | `log_id` | Log |

### 8.5 Face APIs

| URI | Scope | param | result |
|---|---|---|---|
| `/openapi/v1/faces/folders` | `face.read` | `{}` | Folder list |
| `/openapi/v1/faces/list` | `face.read` | `cursor?`, `page_size?` | `items`, `total`, `next_cursor` |
| `/openapi/v1/faces/get` | `face.read` | `face_id` | Face |
| `/openapi/v1/faces/register` | `face.write` | Face | Operation, HTTP 202 |
| `/openapi/v1/faces/update` | `face.write` | Face | Operation, HTTP 202 |
| `/openapi/v1/faces/delete` | `face.write` | `face_id` | `deleted` |
| `/openapi/v1/faces/batch_import` | `face.write` | `items[1..8]` | Operation, HTTP 202 |

Example Face request object:

```json
{
  "face_id": "employee-001",
  "name": "Example Person",
  "work_id": "E001",
  "department": "R&D",
  "phone": "",
  "gender": 0,
  "age": 30,
  "category": 1,
  "photo_base64": "<Base64 without data:image prefix>"
}
```

### 8.6 Retrieval and Upgrade APIs

| URI | Scope | param | result |
|---|---|---|---|
| `/openapi/v1/retrieval/status` | `retrieval.use` | `{}` | Service status |
| `/openapi/v1/retrieval/index/status` | `retrieval.use` | `{}` | Index status |
| `/openapi/v1/retrieval/settings/get` | `retrieval.use` | `{}` | Retrieval settings |
| `/openapi/v1/retrieval/settings/set` | `retrieval.use` | Non-empty settings | Update result |
| `/openapi/v1/retrieval/sync` | `retrieval.use` | Optional synchronization parameters | Synchronization result |
| `/openapi/v1/retrieval/query` | `retrieval.use` | `query`, `top_k?`, `media_scope?` | Search results |
| `/openapi/v1/upgrades/check` | `upgrade.manage` | `{}` | Available releases |
| `/openapi/v1/upgrades/status` | `upgrade.manage` | `{}` | Upgrade status |
| `/openapi/v1/upgrades/apply` | `upgrade.manage` | `kind`, `release_id?` | Start result |
| `/openapi/v1/upgrades/cancel` | `upgrade.manage` | `{}` | Cancel result |

For `retrieval/query`, `top_k` is 1–200 and `media_scope` is `all`, `image`, or `video`. Upgrade `kind` is `app` or `firmware`.

### 8.7 Client Credential Administration (Delivery Team Only)

Exception: `clients/bootstrap` lets customers provision their own credentials with their device web username and password, without a management Token; see 1.1.1. Existing administration APIs and permission boundaries remain unchanged.

Apart from self-service provisioning, third-party customers normally do not call these administration APIs. A delivery administrator uses a management token with `openapi.clients.manage` to create, rotate, or disable customer credentials.

`openapi.clients.manage` and wildcard `*` are reserved for trusted local administrators and cannot be stored in third-party Client `scopes[]`. The server also rejects every `token_use=openapi_client` token on the existing administration APIs below, even if an older database contains a mistaken Scope assignment. Bootstrap authenticates the web credentials; a Token cannot replace the password.

| URI | param | result |
|---|---|---|
| `/openapi/v1/clients/bootstrap` | `username`, `password`, `display_name?`, `scopes?`; HTTPS POST, no management Token | Client and one-time `client_secret`; see 1.1.1 |
| `/openapi/v1/clients/create` | `display_name`, `scopes[]` | Client and one-time `client_secret` |
| `/openapi/v1/clients/list` | `{}` | `items`, `total`; no secret |
| `/openapi/v1/clients/get` | `client_id` | Client; no secret |
| `/openapi/v1/clients/update` | `client_id`, `display_name?`, `scopes?`, `enabled?` | Client |
| `/openapi/v1/clients/rotate_secret` | `client_id` | New `client_secret`; old secret and tokens immediately invalid |

```json
{
  "uri": "/openapi/v1/clients/create",
  "request_id": "req-client-create-001",
  "param": {
    "display_name": "Customer Business Platform",
    "scopes": ["device.read", "task.read", "task.write", "task.execute"]
  }
}
```

Create and rotate are not safe for blind retry; `client_secret` is returned once. If a rotate response is lost, rotate the known `client_id` again. If a create response is lost, call `clients/list`, disable any orphan Client whose secret was not received, then explicitly create a new Client. The device cannot recover the old secret.

### 8.8 Shared Data Models

Unless explicitly stated otherwise, timestamps are Unix milliseconds. Clients must ignore unknown fields added in future compatible releases.

#### 8.8.1 Task

| Field | Type | Required | Description |
|---|---|---|---|
| `task_id` | string | Yes | 1–128 bytes; first character alphanumeric; remaining characters may include `.`, `_`, `:`, `-` |
| `task_name` | string | Yes | 1–256 bytes; no control characters |
| `revision` | int64 | Response | Server concurrency version; starts at 1 and is used by save/delete/start |
| `schema_version` | int | No | Default 1 |
| `runtime_location` | string | No | Default `local`; value must come from `device/runtime_locations` |
| `desired_state` | string | No | `running` or `stopped` |
| `nodes` | array | Yes | Maximum 256 Nodes |
| `edges` | array | Yes | Maximum 1024 Edges; graph must be acyclic |
| `created_at_ms` | int64 | Response | Creation time |
| `updated_at_ms` | int64 | Response | Update time |

Maximum serialized Task size is 2 MiB. A single Node `config` is limited to 1 MiB.

Node:

| Field | Type | Required | Description |
|---|---|---|---|
| `node_id` | string | Yes | Unique within the task; same identifier format as `task_id` |
| `node_type` | string | Yes | Value from `nodes/catalog`; do not hard-code plugin inventory |
| `category` | string | No | `input`, `algorithm`, `process`, `output`, or plugin-provided category |
| `node_schema_version` | int | No | Default 1 |
| `implementation_version` | string | No | Node implementation version |
| `config` | object | Yes | Must conform to `nodes/schema` |

Edge:

| Field | Type | Required | Description |
|---|---|---|---|
| `from` | string | Yes | Upstream `node_id` |
| `from_port` | string | No | Default `default` |
| `to` | string | Yes | Downstream `node_id`; must differ from `from` |
| `to_port` | string | No | Default `default` |

#### 8.8.2 TaskRuntime

| Field | Type | Description |
|---|---|---|
| `task_id` | string | Task ID |
| `state` | string | `starting`, `running`, `stopping`, `stopped`, or `error` |
| `phase` | string | `healthy` while normally running; otherwise usually matches `state` |
| `desired_state` | string | `running` or `stopped` |
| `actual_running` | bool | Device-confirmed actual runtime state |
| `runtime_location` | string | Actual runtime location |
| `started_at_ms` | int64 | Most recent start; may be 0 |
| `stopped_at_ms` | int64 | Most recent stop; may be 0 |
| `updated_at_ms` | int64 | State update time |
| `last_error` | string/null | Most recent runtime error; `null` when absent |

Determine state from `actual_running` and `state`, not only from task-definition `desired_state`.

#### 8.8.3 NodeStatus

| Field | Type | Description |
|---|---|---|
| `node_id` | string | Node ID |
| `node_type` | string | Node type |
| `state` | string | Current aggregate task-runtime state mapped to this node |
| `metrics_available` | bool | Currently `false`; real plugin CPU/FPS/latency metrics are not available |
| `source` | string | Currently `task_runtime_aggregate` |

`tasks/node_metrics` is a reliable task-runtime-to-node snapshot, not a plugin-process health check. `metrics_available=false` is not a node failure.

#### 8.8.4 Operation

| Field | Type | Description |
|---|---|---|
| `operation_id` | string | Asynchronous operation ID |
| `kind` | string | For example `task.start`, `face.register` |
| `resource` | object | `{type,id}` |
| `request_id` | string | Original trace ID |
| `state` | string | `queued`, `running`, `succeeded`, `failed`, `canceled`, `interrupted` |
| `progress` | int | 0–100 |
| `code` / `msg` | int/string | Final business code and message |
| `created_at_ms` | int64 | Creation time |
| `started_at_ms` | int64 | Start time |
| `finished_at_ms` | int64 | Finish time |
| `updated_at_ms` | int64 | Update time |
| `cancelable` | bool | Only a queued Operation can be canceled |
| `result` | any | Present after successful completion |
| `error` | object | Present after failure |

#### 8.8.5 Device, Record, Alarm, Log, and Face

Device stable fields: `device_id`, `guid`, `vendor`, `application_version`, `firmware_version`, `authorization`, `device_time`.

Record stable fields: `record_id`, `task_id`, `task_name`, `filename`, `size_bytes`, `duration_ms`, `modified_at_ms`, `begin_at_ms`, `codec`, `width`, `height`, `mode`, `download_available`; when downloadable, `download_url` and `download_expires_at_ms` may also appear.

Alarm stable fields: `alarm_id`, `task_id`, `task_name`, `alarm_type`, `summary`, `occurred_at`, `image_available`.

Log stable fields: `log_id`, `device_id`, `source`, `level`, `message`, `occurred_at`. The server redacts token/password/secret fragments and truncates long messages to approximately 8 KiB.

Face stable fields: `face_id`, `name`, `gender`, `age`, `work_id`, `id_card_no`, `ic_card_no`, `department`, `phone`, `category`, `photo_available`, `create_time`, `update_time`. Device versions may add read-only fields.

### 8.9 Authentication, Session, WSS, and Client-Credential APIs

#### 8.9.1 Obtain an Access Token

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/auth/token` |
| Purpose | Exchange independent third-party credentials for a short-lived Token; HTTPS only |
| Scope | None |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: required `client_id` (maximum 128) and `client_secret` (maximum 256).

Result: `access_token`, `token_type`, `token_use`, `expires_in`, `expires_at`, `client_id`, `scopes[]`. Authentication failures may trigger 429; wait for `retry_after_ms`.

#### 8.9.2 Get Current Session

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/session/get` |
| Purpose | Confirm Token identity, Scopes, and expiry |
| Scope | Authenticated |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`. Result: `subject`, `client_id?`, `scopes[]`, `expires_at`, `token_use`.

#### 8.9.3 Issue a WebSocket Ticket

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/ws/ticket` |
| Purpose | Issue a one-time WSS Ticket; HTTPS only |
| Scope | Authenticated |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`. Result: `ticket`, `expires_at_ms`, `websocket_path`, `protocol`, `max_message_bytes`, `heartbeat_interval_ms`.

Connect to `wss://<device-host>:8099<websocket_path>?ticket=<ticket>`. Never reuse the Ticket or write it to ordinary logs.

The following five APIs are for trusted local delivery administrators only. They require HTTPS and `openapi.clients.manage`. An `openapi_client` Token can never call them. Cloud tenant administrators must use the Device Gateway's separate management plane.

#### 8.9.4 Create a Third-Party Client

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/clients/create` |
| Purpose | Create a third-party Client |
| Scope | `openapi.clients.manage` |
| Idempotency key | Not supported |
| Success | `HTTP 200 OK` |

Parameters: required `display_name` (1–128 bytes), `scopes[]` (1–64 entries). Result: Client fields plus `client_secret` and `secret_returned_once=true`.

The secret is returned once. Do not blindly retry after timeout. First call `clients/list`, disable any orphan Client whose secret was not received, and then create a new one.

#### 8.9.5 List Third-Party Clients

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/clients/list` |
| Purpose | List Clients without secrets |
| Scope | `openapi.clients.manage` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`. Result: `items[]`, `total`. Each item includes `client_id`, `display_name`, `scopes`, `enabled`, `created_at_ms`, `updated_at_ms`, `last_used_at_ms`, `last_rotated_at_ms`.

#### 8.9.6 Get a Third-Party Client

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/clients/get` |
| Purpose | Get one Client without its secret |
| Scope | `openapi.clients.manage` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameter: required `client_id`. Result: Client.

#### 8.9.7 Update a Third-Party Client

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/clients/update` |
| Purpose | Change display name, Scopes, or enabled state |
| Scope | `openapi.clients.manage` |
| Idempotency key | Not supported |
| Success | `HTTP 200 OK` |

Parameter: required `client_id`; provide at least one of `display_name`, `scopes[]`, `enabled`. Result: updated Client. Changing Scopes or enabled state invalidates all current tokens for that Client.

#### 8.9.8 Rotate a Client Secret

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/clients/rotate_secret` |
| Purpose | Rotate a Client secret |
| Scope | `openapi.clients.manage` |
| Idempotency key | Not supported |
| Success | `HTTP 200 OK` |

Parameter: required `client_id`. Result: `client_id`, new `client_secret`, `secret_returned_once=true`, `rotated_at_ms`. The old secret and all existing tokens are invalid immediately. If the successful response is lost, rotate the same Client again and retain only the last returned secret.

### 8.10 Operation APIs

Asynchronous mutation APIs return HTTP 202 and an Operation. An Operation is visible only to the authenticated subject that submitted it, except for a trusted wildcard administrator. Operation records are retained for seven days after their last update. Persist `operation_id` immediately, obtain the terminal state through `operation.state.changed` or polling, and store the business result in the customer system. The device Operation table is not a long-term audit store.

#### 8.10.1 Get an Operation

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/operations/get` |
| Purpose | Read one asynchronous Operation |
| Scope | Authenticated; no additional Scope |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameter: required `operation_id`. Result: Operation.

#### 8.10.2 List Operations

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/operations/list` |
| Purpose | List recent Operations owned by the current subject |
| Scope | Authenticated; no additional Scope |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameter: optional `page_size` 1–200, default 50. Result: `items[]` (Operation), `count`.

#### 8.10.3 Cancel an Operation

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/operations/cancel` |
| Purpose | Cancel an Operation that has not started |
| Scope | Authenticated; no additional Scope |
| Idempotency key | Required |
| Success | `HTTP 200 OK` |

Parameter: required `operation_id`. Result: canceled Operation. Only `queued` can be canceled; other states return `40903`.

### 8.11 Event APIs

One subscription contains 1–24 Topics. One subject may hold at most eight active subscriptions; an idle subscription expires after 30 minutes. If any Topic is missing its required Scope, the whole subscribe request returns HTTP 403 and no partial subscription is created.

Event envelope fields are `type,event_id,sequence,cursor,topic,occurred_at_ms,resource{type,id},data`; WSS pushes also include `subscription_id`.

| Field | Type | Description |
|---|---|---|
| `type` | string | `event` for WSS push |
| `subscription_id` | string | Delivering subscription; may be absent from HTTPS poll `items[]` |
| `event_id` | string | Global event identity used for deduplication |
| `sequence` | int64 | Monotonic within the current process journal; not continuous across restart |
| `cursor` | string | Opaque recovery position; store and return unchanged |
| `topic` | string | Event Topic |
| `occurred_at_ms` | int64 | Event creation time; may differ from business occurrence time |
| `resource.type` | string | For example `alarm`, `task_nodes`, `device` |
| `resource.id` | string | Resource ID; an alarm ID for alarms, task ID for node events |
| `data` | object | Lightweight Topic-specific notification, not an authoritative snapshot |

Stable `data` fields by Topic:

| Topic | Stable `data` fields | Notes |
|---|---|---|
| `alarm.created` | `task_id,alarm_type,image_available` | Image bytes are never included |
| `alarm.deleted` | `task_id` | `resource.id` is the deleted alarm ID |
| `record.created`, `record.deleted` | `task_id,filename` | Safe filename only; call `records/get` |
| `task.created`, `task.updated` | `task_id,task_name,revision,desired_state,updated_at_ms,snapshot_required` | Read `tasks/get` |
| `task.deleted` | `task_id,deleted,snapshot_required` | `deleted=true`, `snapshot_required=false` |
| `task.state.changed` | TaskRuntime fields and `reason`, or `operation_id,operation_kind,state,code,msg,result?,error?` | Always confirm with `tasks/runtime` |
| `node.state.changed` | `task_id,state,snapshot_required,snapshot_uri,reason?,operation_id?` | Read the complete `tasks/node_metrics` snapshot |
| `plugin.catalog.changed` | `reason,revision,ready,discovered,settled,snapshot_required,snapshot_uri,plugin_type?` | `plugin_type` is optional |
| `device.state.changed` | `state,control_plane_state,detail` | Only `state=offline/connecting/online` is a stable business enum |
| `face.library.changed` | `reason,affected_count,snapshot_required,snapshot_uri,folders_snapshot_uri,folder_id?` | No identity data, photos, or vectors |
| `upgrade.catalog.changed` | `status,message,checked_at,app,firmware,snapshot_required,snapshot_uri` | Candidate objects are allowlisted summaries |
| `upgrade.state.changed` | `reason,kind,release_id,phase,progress,active,current_version,target_version,message,started_at,updated_at,finished_at,snapshot_required,snapshot_uri` | Read `upgrades/status` |
| `retrieval.settings.changed`, `retrieval.sync.queued` | `reason,success,enabled,started,applying,sync_queued,indexing,index_size,meta_size,total_sync_rounds,total_sync_failures,last_sync_started_ms,last_sync_finished_ms,last_sync_stage,snapshot_required,snapshot_uri` | No query, path, or hit data |
| `retrieval.index.changed` | Previous counters plus `alarm_upsert_ok,alarm_upsert_fail,record_index_ok,record_index_fail` | Read `retrieval/index/status` |
| `operation.state.changed` | Complete Operation | Subject-private |

The bounded in-process journal holds at most 1024 events and 4 MiB. Delivery is at least once. Realtime arrival order does not replace `sequence`; deduplicate and ACK only a contiguous position. A restart or an old cursor returns `40904 EVENT_CURSOR_EXPIRED`; rebuild snapshots and subscribe from a new cursor.

#### 8.11.1 Subscribe to Events

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/events/subscribe` |
| Purpose | Create or recover a subscription |
| Scope | Determined by Topics |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: required `topics[]` with 1–24 entries; optional `cursor`, `subscription_id`. Duplicate Topics are deduplicated.

Result: `subscription_id`, `topics[]`, `delivery`, `replay_items[]`, `next_cursor`, `expires_in_ms`.

`delivery` is `poll` over HTTPS and `realtime+poll` over WSS. `operation.state.changed` is always subject-private. A WSS subscription is bound to the connection; after close, create a new subscription with the last cursor, not the old ID. `replay_items[]` contains at most 100 entries; poll until `has_more=false`. WSS heartbeat does not renew the subscription TTL; poll or resubscribe before expiry.

#### 8.11.2 Poll Events

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/events/poll` |
| Purpose | Page through events for HTTPS delivery or WSS recovery |
| Scope | Determined by subscription Topics |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: required `subscription_id`; optional `cursor`; optional `page_size` 1–100, default 100. Result: `subscription_id`, `items[]`, `next_cursor`, `has_more`. ACK a contiguous `next_cursor` only after all corresponding items have been committed.

#### 8.11.3 Acknowledge an Event Cursor

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/events/ack` |
| Purpose | Confirm the highest contiguous business-processed position |
| Scope | Determined by subscription Topics |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: required `subscription_id`, `cursor`. Result: `subscription_id`, `acknowledged_cursor`.

#### 8.11.4 Unsubscribe

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/events/unsubscribe` |
| Purpose | Release a subscription |
| Scope | Determined by subscription Topics |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameter: required `subscription_id`. Result: `subscription_id`, `deleted=true`.

### 8.12 Device, Node-Catalog, and Plugin APIs

#### 8.12.1 Get Device Information

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/device/get` |
| Purpose | Read device identity, software/firmware versions, authorization, and time |
| Scope | `device.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`. Result: Device (Section 8.8.5).

#### 8.12.2 Get Device Metrics

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/device/metrics` |
| Purpose | Read compute, memory, temperature, and storage snapshot |
| Scope | `device.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`. Result: `collected_at_ms`, `compute[]`, `storage[]`.

Each `compute[]` item includes `name`, `cpu_percent`, `npu_percent`, `memory_percent`, `memory_total_bytes`, `memory_used_bytes`, `cmm_percent`, `cmm_total_bytes`, `cmm_used_bytes`, `temperature_celsius`. Each `storage[]` item includes `storage_id`, `total_bytes`, `used_bytes`. A percentage can briefly be zero; use consecutive samples rather than a single point for fault detection.

#### 8.12.3 List Runtime Locations

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/device/runtime_locations` |
| Purpose | List task deployment locations |
| Scope | `device.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`. Result: `items[]`; each item has `label`, `value`, `device_id`, `kind`, where `kind` is `local` or `compute_card`. Store `value` in Task `runtime_location`.

#### 8.12.4 Get Device Capabilities

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/device/capabilities` |
| Purpose | Capability negotiation during client initialization |
| Scope | `device.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`. Result: `runtime_locations[]`, `node_types[]`, `features[]`.

`features[]` is currently a high-level UI-degradation hint, not a complete URI inventory. It may include `tasks`, `task_operations`, `alarms`, `plugin_inventory`, `upgrade_status`, `retrieval_status`; published domains such as recordings, faces, logs, and events may not have individual entries. Use this protocol to determine published APIs and `session/get.scopes[]` plus the actual response to determine current authorization. Never delete customer data merely because a Feature hint is absent, and never bypass Scope checks because it is present.

#### 8.12.5 Get Network State

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/device/network/get` |
| Purpose | Read network-interface snapshot; no network mutation |
| Scope | `device.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`. Result: `interfaces[]`; each contains `name`, `mode`, `ip`, `gateway`, `netmask`, `dns`; `mode` is `dhcp` or `static`.

#### 8.12.6 Get the Node Catalog

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/nodes/catalog` |
| Purpose | Read available nodes and loading progress during asynchronous plugin discovery |
| Scope | `task.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`. Result: `groups[]`, `revision`, `ready`, `discovered`, `settled`.

Render available `groups` immediately. While `ready=false` or `discovered!=settled`, retry with backoff. When `revision` changes, replace the old catalog atomically. Never truncate by a fixed plugin count or hard-code a previously observed 17/30 count.

#### 8.12.7 Get a Node Schema

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/nodes/schema` |
| Purpose | Read configuration Schema/form metadata for one node type |
| Scope | `task.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameter: required `node_type` from the catalog. Result: `node_type`, dynamic `schema`. Preserve unknown Schema fields, generate the form from the current device response, and refetch after plugin upgrade.

#### 8.12.8 Validate a Task Graph

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/graphs/validate` |
| Purpose | Validate a complete task before save/start |
| Scope | `task.write` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameter: complete `task`. Result: `valid=true`, `warnings[]`. Validation failure returns `42201` and may include `field`, `node_id`, `edge_index`, `detail`. Checks cover identifiers, size/count limits, references, self-loops, duplicate edges, cycles, and JSON size.

#### 8.12.9 List Plugins

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/plugins/list` |
| Purpose | Read plugin inventory, versions, load state, authorization/signature state, and task references |
| Scope | `task.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`. Result: `currentPlatform`, `items[]`, `marketplaceReady`, `total`.

Stable item fields include `id`, `type`, `name`, `label`, `labelText`, `version`, `description`, `vendor`, `developer`, `enabled`, `loaded`, `state`, `status`, `compatible`, `source`, `entry`, `packageName`, `packageSize`, `md5`, `buildTime`, `installedAt`, `updatedAt`, `loadTimeMs`, `errorCount`, `lastError`, `inUse`, `inUseByRunningTasks`, `referenceCount`, `runningReferenceCount`, `references[]`, `runningReferences[]`, `restartRequired`, `platform`, `currentPlatform`. Commercial license/signature metadata may add fields.

#### 8.12.10 Get Plugin Health

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/plugins/health` |
| Purpose | Read a compact plugin-health snapshot for monitoring |
| Scope | `task.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`. Result: `items[]`, `total`.

Each item contains `type`, `version`, `enabled`, `state`, `loadTimeMs`, `errorCount`, `lastError`, and `taskRefCount`. If an action is pending, it also contains `pendingAction{type,reason,scheduledAt}`. `state` is one of `loaded`, `disabled`, `error`, or `installed`.

### 8.13 Task APIs

#### 8.13.1 List Tasks

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/tasks/list` |
| Purpose | Page through task definitions ordered by descending `updated_at_ms` |
| Scope | `task.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: optional `cursor`; `page_size` 1–200, default 50; optional `name` substring filter. Result: `items[]` (Task), `total`, `has_more`, `next_cursor`.

#### 8.13.2 Get a Task Definition

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/tasks/get` |
| Purpose | Read the complete Task and current Revision |
| Scope | `task.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameter: required `task_id`. Result: Task.

#### 8.13.3 Create a Task

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/tasks/create` |
| Purpose | Create a Task |
| Scope | `task.write` |
| Idempotency key | Required |
| Success | `HTTP 200 OK` |

Parameter: complete `task`; supplied read-only timestamps and Revision are ignored. Result: created Task with `revision=1`. An existing `task_id` returns a resource-state conflict.

#### 8.13.4 Save a Task

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/tasks/save` |
| Purpose | Fully replace an existing Task definition |
| Scope | `task.write` |
| Idempotency key | Required |
| Success | `HTTP 200 OK` |

Parameters: complete `task`, positive `expected_revision`. Result: saved Task with incremented Revision. Read with `tasks/get` first; on `40901`, refetch and resolve the conflict instead of overwriting.

#### 8.13.5 Clone a Task

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/tasks/clone` |
| Purpose | Clone a Task into stopped state |
| Scope | `task.write` |
| Idempotency key | Required |
| Success | `HTTP 200 OK` |

Parameters: required `source_task_id`, `new_task_id`; optional `new_task_name`. Result: new Task with `revision=1`, `desired_state=stopped`.

#### 8.13.6 Delete a Task

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/tasks/delete` |
| Purpose | Delete a Task definition |
| Scope | `task.write` |
| Idempotency key | Required |
| Success | `HTTP 200 OK` |

Parameters: required `task_id`, `expected_revision`. Result: `task_id`, `deleted=true`. Stop the Task and confirm `actual_running=false` before deletion.

#### 8.13.7 Start a Task

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/tasks/start` |
| Purpose | Asynchronously start the specified Revision |
| Scope | `task.execute` |
| Idempotency key | Required |
| Success | `HTTP 202 Accepted` |

Parameters: required `task_id`, `revision`. The HTTP result is an Operation; after successful completion, Operation `result` is TaskRuntime.

#### 8.13.8 Stop a Task

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/tasks/stop` |
| Purpose | Asynchronously stop a Task |
| Scope | `task.execute` |
| Idempotency key | Required |
| Success | `HTTP 202 Accepted` |

Parameter: required `task_id`. The HTTP result is an Operation; successful Operation `result` is TaskRuntime.

#### 8.13.9 Restart a Task

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/tasks/restart` |
| Purpose | Stop, then start the specified Revision |
| Scope | `task.execute` |
| Idempotency key | Required |
| Success | `HTTP 202 Accepted` |

Parameters: required `task_id`, `revision`. The HTTP result is an Operation; successful Operation `result` is TaskRuntime.

#### 8.13.10 Save and Start a Task

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/tasks/apply_and_start` |
| Purpose | Save a new definition and start it as one business workflow |
| Scope | Both `task.write` and `task.execute` |
| Idempotency key | Required |
| Success | `HTTP 202 Accepted` |

Parameters: complete `task`, `expected_revision`. The HTTP result is an Operation; successful Operation `result` contains the new `task` and `runtime` (TaskRuntime).

#### 8.13.11 Get Task Runtime

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/tasks/runtime` |
| Purpose | Read actual Task runtime state |
| Scope | `task.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameter: required `task_id`. Result: TaskRuntime.

#### 8.13.12 Get Task Node State

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/tasks/node_metrics` |
| Purpose | Read Task and all node states in one call |
| Scope | `task.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameter: required `task_id`. Result: `task_id`, `state`, `desired_state`, `actual_running`, `runtime_location`, `last_error`, `collected_at_ms`, `nodes[]` (NodeStatus).

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

#### 8.13.13 Device, Task, and Node Refresh Guidance

| Page data | Initialization | Recommended refresh |
|---|---|---|
| Device versions/authorization | `device/get` | On page entry and version change |
| CPU/NPU/memory/temperature/storage | `device/metrics` | Every 3–5 s while visible; pause/slow in background |
| Task definitions | `tasks/get` / `tasks/list` | On entry, after save, and after Revision conflict |
| Task runtime | `tasks/runtime` | 500 ms–1 s during transition; 3–5 s when stable |
| Node state | `tasks/node_metrics` | Same rate as Task runtime; do not poll per node |
| Plugin loading | `nodes/catalog` | Backoff while loading; stop when `ready=true` |

Add 10%–20% jitter to HTTP polling and slow down when the page is hidden. Prefer WSS for new alarms and recordings instead of high-frequency list scans.

### 8.14 Media Output, Recording, Alarm, and Log APIs

#### 8.14.1 Get Task Media Outputs

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/media/outputs` |
| Purpose | Read RTSP URLs produced by Task output nodes |
| Scope | `media.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameter: required `task_id`. Result: `task_id`, `items[]`. Each item contains `output_id`, `node_id`, `protocol`, `enabled`, `codec`, `port`, `url`, `urls[]`, `credentials_included`, `updated_at_ms`. The server strips URL usernames/passwords and returns `credentials_included=false`. `items` may be empty while output is not ready; also check Task runtime.

#### 8.14.2 List Recordings

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/records/list` |
| Purpose | Page through recordings |
| Scope | `media.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: optional `cursor`; `page_size` 1–100, default 50; optional `task_id`, `year`, `month`, `day`. Result: `items[]` (Record), `total`, `total_bytes`, `next_cursor`.

#### 8.14.3 Get a Recording

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/records/get` |
| Purpose | Read recording metadata |
| Scope | `media.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameter: required `record_id` in `rec_` + 64 hex format. Result: Record.

#### 8.14.4 Issue a Recording Download Ticket

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/records/download_ticket` |
| Purpose | Issue a short-lived recording download URL |
| Scope | `media.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameter: required `record_id`. Result: `record_id`, relative `url`, `expires_at_ms`, `range_supported=true`. Resolve the URL against `https://<device-host>:8099`; HTTP Range is supported. Obtain a new Ticket after expiry.

#### 8.14.5 Delete a Recording

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/records/delete` |
| Purpose | Delete the recording file and record |
| Scope | `media.manage` |
| Idempotency key | Required |
| Success | `HTTP 200 OK` |

Parameter: required `record_id`. Result: `record_id`, `deleted=true`. Deletion is irreversible; require customer confirmation.

#### 8.14.6 List Alarms

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/alarms/list` |
| Purpose | Page through device alarms |
| Scope | `alarm.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: optional `cursor`; `page_size` 1–200, default 50; optional `task_name`. Result: `items[]` (Alarm), `total`, `next_cursor`.

#### 8.14.7 Get an Alarm

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/alarms/get` |
| Purpose | Read one alarm |
| Scope | `alarm.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameter: required positive-integer or decimal-string `alarm_id`. Result: Alarm.

#### 8.14.8 Delete an Alarm

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/alarms/delete` |
| Purpose | Delete an alarm record |
| Scope | `alarm.manage` |
| Idempotency key | Required |
| Success | `HTTP 200 OK` |

Parameter: required `alarm_id`. Result: `alarm_id`, `deleted=true`.

#### 8.14.9 List Logs

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/logs/list` |
| Purpose | Page through device logs |
| Scope | `log.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: optional `cursor`; `page_size` 1–100, default 50; optional `source` (max 128), `level` (max 32), `keyword` (max 256), `start_time`, `end_time` (max 64 each). Result: `items[]` (Log), `total`, `next_cursor`. Time-filter format follows the device log source; use the format actually returned by the target device.

#### 8.14.10 Get a Log Entry

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/logs/get` |
| Purpose | Read one log entry |
| Scope | `log.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameter: required positive-integer or decimal-string `log_id`. Result: Log.

### 8.15 Face-Library APIs

Face mutation fields:

| Field | Type | Required | Limit |
|---|---|---|---|
| `face_id` | string | Yes | 1–128; ASCII letters, digits, `_`, `-`, `.`, `:`; no spaces |
| `name` | string | No | 128 bytes |
| `work_id` | string | No | 128 bytes |
| `id_card_no` | string | No | 128 bytes |
| `ic_card_no` | string | No | 128 bytes |
| `department` | string | No | 128 bytes |
| `phone` | string | No | 64 bytes |
| `gender` | int | No | 0–2 |
| `age` | int | No | 0–150 |
| `category` | int | No | 0–2 |
| `photo_base64` | string | Register/update | No `data:image/...;base64,` prefix; maximum encoded length 2,800,000 bytes |

#### 8.15.1 List Face Folders

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/faces/folders` |
| Purpose | List face groups |
| Scope | `face.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`. Result: `items[]`, `total`, `next_cursor`; the current default group contains `folder_id`, `name`, `face_count`.

#### 8.15.2 List Faces

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/faces/list` |
| Purpose | Page through Face metadata without original photo Base64 |
| Scope | `face.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: optional `cursor`; `page_size` 1–32, default 32. Result: `items[]` (Face), `total`, `next_cursor`. Keep the same `page_size` across pages or receive `CURSOR_PAGE_SIZE_MISMATCH`.

#### 8.15.3 Get a Face

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/faces/get` |
| Purpose | Read one Face by ID |
| Scope | `face.read` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameter: required `face_id`. Result: Face. `photo_available` only reports existence; it does not return the photo.

#### 8.15.4 Register a Face

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/faces/register` |
| Purpose | Asynchronously register one Face |
| Scope | `face.write` |
| Idempotency key | Required |
| Success | `HTTP 202 Accepted` |

Parameter: complete Face mutation object including `photo_base64`. HTTP result: Operation; successful Operation result contains `face_id`.

#### 8.15.5 Update a Face

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/faces/update` |
| Purpose | Asynchronously replace Face metadata and photo |
| Scope | `face.write` |
| Idempotency key | Required |
| Success | `HTTP 202 Accepted` |

Parameter: complete Face mutation object; the current version still requires `photo_base64`. HTTP result: Operation; successful result contains `face_id`.

#### 8.15.6 Delete a Face

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/faces/delete` |
| Purpose | Delete Face metadata and managed image |
| Scope | `face.write` |
| Idempotency key | Required |
| Success | `HTTP 200 OK` |

Parameter: required `face_id`. Result: `face_id`, `deleted=true`.

#### 8.15.7 Batch Import Faces

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/faces/batch_import` |
| Purpose | Asynchronously register a batch of Faces |
| Scope | `face.write` |
| Idempotency key | Required |
| Success | `HTTP 202 Accepted` |

Parameter: `items[]` containing 1–8 complete Face objects; `face_id` must be unique within the batch; combined Base64 maximum is 3 MiB.

HTTP result: Operation. Successful Operation `result` includes `atomic=false`, `total`, `succeeded`, `failed`, `items[]`; each item has `face_id`, `state`, and failed entries include `code`, `msg`.

The batch is non-atomic. Retry only failed items with a new idempotency key. The device has a bounded 32 MiB concurrent Face-Base64 budget; excess requests return 429.

### 8.16 Intelligent Retrieval APIs

#### 8.16.1 Get Retrieval Service Status

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/retrieval/status` |
| Purpose | Read the combined state of the retrieval service, work queue, and index |
| Scope | `retrieval.use` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`.

The result contains the following stable field groups:

- Service: `enabled`, `started`, `service_state_code`, `service_state_text`, `service_state_level`, `service_state_detail`, `service_state_hint`, `warming_up`, `bootstrap_failed`, `last_error`.
- Queue: `worker_busy`, `active_job`, `job_queue_size`, `max_job_queue_size`.
- Index: `index_size`, `meta_size`, `image_meta_count`, `video_meta_count`, `indexing`, `index_dirty`, `pending_index_ops`, `last_index_flush_ms`, `last_search_snapshot_refresh_ms`.
- Synchronization: `sync_queued`, `last_sync_stage`, `last_sync_detail`, `last_sync_started_ms`, `last_sync_finished_ms`, `total_sync_rounds`, `total_sync_failures`.
- Runtime configuration: `run_location`, `runtime_location`, `infer_type`, `infer_device_id`, `feature_dim`, and the current model/database and threshold fields.

The device may add diagnostic fields for different models or software versions; clients must ignore unknown fields. A status snapshot may currently include model, database, or index paths. These path fields are not a stable business contract: do not display them, construct download URLs or file operations from them, or write them to ordinary business logs. A later version may remove or redact them.

#### 8.16.2 Get Retrieval Index Status

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/retrieval/index/status` |
| Purpose | Read index status; currently returns the same full snapshot as `retrieval/status` and may evolve independently |
| Scope | `retrieval.use` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`. Result: the same snapshot as `retrieval/status`.

#### 8.16.3 Get Retrieval Settings

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/retrieval/settings/get` |
| Purpose | Read persisted settings, runtime settings, and available execution locations |
| Scope | `retrieval.use` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`.

The result contains `success`, `config_path`, `config`, `persisted_config`, `run_location_options`, `runtime_location_options`, `warming_up`, and `bootstrap_failed`. It may contain `persisted_config_error` or `bootstrap_error` when loading fails.

Stable `config` fields are:

`config_version`, `enabled`, `run_location`, `runtime_location`, `cold_search_page_size`, `hot_index_vectors`, `max_active_vectors`, `max_frames_per_video`, `max_frames_per_video_hard_limit`, `min_score_image`, `min_score_text`, `sample_frame_interval`, and `video_frame_dedup_threshold`.

`config_path` and any model, database, or index paths in `config` or `persisted_config` are device diagnostic fields, not customer-configurable file APIs. Do not display, persist a dependency on, or send these paths back to the device. Send only the stable settings listed above when updating configuration.

#### 8.16.4 Update Retrieval Settings

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/retrieval/settings/set` |
| Purpose | Persist and apply retrieval settings |
| Scope | `retrieval.use` |
| Idempotency key | Required |
| Success | `HTTP 200 OK` |

Parameter: a non-empty configuration object. Stable settings may be passed directly or in a `config` object matching the device response. Change only fields that are actually required.

The result contains `success`, `saved`, `applied`, `applying`, `message`, `config_path`, `config`, `runtime_config`, `run_location_options`, and `runtime_location_options`; a failure may include `apply_error`.

Changing a model or execution location may start asynchronous warm-up. Poll `retrieval/status` until `warming_up=false` or an explicit failure is reported. The returned `config_path` is for controlled diagnostics only; never use it as a later request parameter, download URL, or customer setting.

#### 8.16.5 Trigger Retrieval Index Synchronization

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/retrieval/sync` |
| Purpose | Trigger synchronization of media metadata and the vector index |
| Scope | `retrieval.use` |
| Idempotency key | Required |
| Success | `HTTP 200 OK` |

Parameter: `{}` triggers the default synchronization. Additional options supported by a device version will be advertised in a response or capability; do not invent unknown fields.

The result reports acceptance/queue state, commonly `success`, `queued`, `message`, and the current synchronization stage. Poll `retrieval/status` fields `sync_queued`, `indexing`, and `last_sync_*`; do not continuously resubmit synchronization requests.

#### 8.16.6 Execute an Intelligent Retrieval Query

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/retrieval/query` |
| Purpose | Search images and recordings using natural language |
| Scope | `retrieval.use` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

| Parameter | Type | Required | Constraint |
|---|---|---|---|
| `query` | string | Yes | 1–2048 bytes |
| `top_k` | int | No | 1–200; default 20 |
| `media_scope` | string | No | `all`, `image`, or `video` |

The result contains `success`, `results[]`, and device-provided timing or pagination diagnostics. Stable result-hit fields include `score`, source identifier/type, and `media_reference`; a hit may also contain `preview_reference` and task, timestamp, frame number, or recording-offset metadata.

`media_reference` and `preview_reference` are controlled media references, not device absolute file paths. If the service is not ready, follow the returned `retryable` flag and status guidance.

### 8.17 Online Upgrade APIs

Upgrade fields use the existing naming convention. Numeric `checkedAt`, `startedAt`, `updatedAt`, and `finishedAt`, and the corresponding snake_case fields in upgrade event `data`, are Unix seconds. `publishedAt` is normally an ISO 8601 string supplied by the upgrade manifest. The outer event field `occurred_at_ms` remains Unix milliseconds. Clients must convert each field according to its definition and must not display a seconds value as milliseconds.

#### 8.17.1 Check for Available Upgrades

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/upgrades/check` |
| Purpose | Refresh and read available application/firmware versions |
| Scope | `upgrade.manage` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`.

Top-level result fields:

| Field | Type | Description |
|---|---|---|
| `deviceId` | string | Device identifier |
| `company` | string | Vendor identifier |
| `current` | object | Current application and firmware versions |
| `capabilities` | object | Device upgrade capabilities |
| `check` | object | Latest check state and upgrade-source information |
| `app` | object | Application upgrade candidate |
| `firmware` | object | Firmware upgrade candidate |
| `runningTask` | object | Running-task summary used for pre-upgrade checks |

`current` and `capabilities`:

| Object | Field | Type | Description |
|---|---|---|---|
| `current` | `appVersion` | string | Current application version |
| `current` | `firmwareVersion` | string | Current firmware version |
| `capabilities` | `appTarget` | string | Application installation target |
| `capabilities` | `firmwareTarget` | string | Firmware installation target |
| `capabilities` | `supportsFirmwareUpgrade` | bool | Whether firmware online upgrade is supported |

`check` fields:

| Field | Type | Description |
|---|---|---|
| `status` | string | Check state |
| `message` | string | State description |
| `checkedAt` | string/int64 | Most recent check time |
| `autoCheckEnabled` | bool | Whether automatic checking is enabled |
| `autoCheckIntervalSec` | int | Automatic check interval in seconds |
| `checkUrl` | string | Check API address |
| `manifestUrl` | string | Upgrade manifest address |

Common `app`/`firmware` candidate fields:

| Field | Type | Description |
|---|---|---|
| `kind` | string | `app` or `firmware` |
| `supported` | bool | Whether this device supports the upgrade type |
| `currentVersion` | string | Current version |
| `latestVersion` | string | Latest version |
| `hasUpdate` | bool | Whether an upgrade is available |
| `releaseId` | string | Release identifier used by `upgrades/apply` |
| `releaseNotes` | string | Release notes |
| `publishedAt` | string/int64 | Publication time |
| `size` | int64 | Package size in bytes |
| `sha256` | string | Package SHA-256 |
| `mandatory` | bool | Whether the upgrade is mandatory |
| `phase` | string | Current phase |
| `progress` | number | Progress percentage |
| `message` | string | State description |
| `checkedAt` | string/int64 | Check time |

A device version may add optional download-target or other candidate fields. Clients must ignore unknown fields.

#### 8.17.2 Get Upgrade Status

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/upgrades/status` |
| Purpose | Read current upgrade progress |
| Scope | `upgrade.manage` |
| Idempotency key | Not required |
| Success | `HTTP 200 OK` |

Parameters: `{}`.

| Field | Type | Description |
|---|---|---|
| `active` | bool | Whether an active upgrade exists |
| `kind` | string | `app` or `firmware` |
| `releaseId` | string | Release identifier |
| `phase` | string | Current phase; see below |
| `progress` | number | Progress percentage |
| `currentVersion` | string | Current version |
| `targetVersion` | string | Target version |
| `message` | string | Current state or failure reason |
| `startedAt` | string/int64 | Start time |
| `updatedAt` | string/int64 | Last update time |
| `finishedAt` | string/int64 | Completion time |
| `filename` | string | Package filename |
| `sha256` | string | Package SHA-256 |

| `phase` | Meaning |
|---|---|
| `idle` | No upgrade job |
| `pending` | Waiting to run |
| `downloading` | Downloading |
| `verifying` | Verifying the package |
| `installing` | Installing |
| `restarting` | Restarting a related service or the device |
| `success` | Upgrade succeeded |
| `failed` | Upgrade failed; inspect `message` |
| `cancelled` | Upgrade was cancelled |

A device version may add download source, installation verification, or exit-code fields.

#### 8.17.3 Apply an Online Upgrade

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/upgrades/apply` |
| Purpose | Start an application or firmware upgrade |
| Scope | `upgrade.manage` |
| Idempotency key | Required |
| Success | `HTTP 200 OK` |

| Parameter | Type | Required | Description |
|---|---|---|---|
| `kind` | string | Yes | `app` or `firmware` |
| `release_id` | string | No | Release identifier; use the value from `upgrades/check` |

Result: `upgrade`, containing the current upgrade status described in 8.17.2.

Before applying, verify device capability, target version, SHA-256, and running-task state. Continue polling upgrade status while it runs. If an HTTP timeout occurs, retry with the same idempotency key; do not generate a new one.

#### 8.17.4 Cancel an Online Upgrade

| Property | Value |
|---|---|
| Command URI | `POST /openapi/v1/upgrades/cancel` |
| Purpose | Cancel an upgrade that is still in a cancellable phase |
| Scope | `upgrade.manage` |
| Idempotency key | Required |
| Success | `HTTP 200 OK` |

Parameters: `{}`. Result: `upgrade`, containing the post-cancellation status described in 8.17.2.

An upgrade may no longer be cancellable after entering installation or restart. In that case, follow the returned error and current status.

## 9. Pagination, Idempotency, and Retry

### 9.1 Pagination

```json
{"page_size":50,"cursor":"aibox-v1-50"}
```

Omit `cursor` on the first request. On subsequent requests, use the returned `next_cursor` unchanged; do not parse or modify it.

A pagination cursor does not guarantee a database snapshot across a device restart or large data changes. Resources may be added or removed during traversal. Deduplicate by resource primary key and, for continuous synchronization, combine realtime events with periodic full reconciliation instead of relying on one long pagination pass.

### 9.2 Write APIs That Require an Idempotency Key

```text
tasks/create, tasks/save, tasks/delete, tasks/clone
tasks/start, tasks/stop, tasks/restart, tasks/apply_and_start
operations/cancel
records/delete, alarms/delete
faces/register, faces/update, faces/delete, faces/batch_import
upgrades/apply, upgrades/cancel
retrieval/settings/set, retrieval/sync
```

When retrying the same business request after a timeout, reuse the same `idempotency_key`. Generate a new key when business parameters change.

Idempotency records are isolated by authenticated subject and retained for 24 hours. During retention, the same subject, key, URI, and parameters return the original result; reusing the key for a different request returns `40902`. After 24 hours, do not depend on the device remembering the key. The customer platform must retain its own business-request state to prevent duplicate execution.

### 9.3 Retry Guidance

| Condition | Action |
|---|---|
| Network timeout or explicit `retryable=true` | Retry with exponential backoff |
| `40101` | Obtain a new Access Token |
| `40301` | Obtain the required Scope; do not retry unchanged |
| `40901` | Refetch the Task and resolve the Revision conflict |
| `40904` | Discard the old cursor, rebuild the snapshot, and resubscribe |
| `40905` | Refetch resource state before deciding the next action; do not run event-cursor recovery |
| `42901` | Wait for the server-advised interval and reduce concurrency |
| Other 4xx | Correct the request before calling again |

Use a 10-second timeout for ordinary requests and 30 seconds for recording, log, or node-catalog requests. For asynchronous APIs, wait only for HTTP `202`, then poll the Operation; do not keep a long HTTP request open. HTTP `429` returns both JSON `retry_after_ms` and the standard `Retry-After` header. Use the more conservative delay and add random jitter.

### 9.4 Third-Party Entry-Point Resource Budgets

Third-party OpenAPI uses scheduling budgets independent of the local web UI and CloudWS. Current single-device defaults are:

| Resource | Default third-party OpenAPI limit |
|---|---:|
| Concurrent executing requests | Global 4; per Client 2 |
| Request rate | Per Client 60 requests/second |
| Mutation rate | Per Client 10 requests/second |
| Active Access Tokens | 2048 across all OpenAPI Clients; 16 per Client |
| Asynchronous Operations | Independent queue of 32; at most 8 unfinished per Client |
| Operation workers | One independent worker; does not consume existing control workers |
| Event subscriptions | 128 across OpenAPI; 8 per authenticated subject |
| WSS connections | 4 per authenticated subject; 64 server-wide |
| Unconsumed WSS Tickets | 8 per authenticated subject; 1024 server-wide; each is single-use and valid for 60 seconds |
| WSS message rate | 100 messages/second per connection; the server closes connections that exceed it |

Exceeding a limit returns `42901` and does not queue work on local-web or CloudWS control resources. Delivery builds may lower these limits. Clients must not depend on high-frequency polling as a substitute for events.

## 10. Error Codes

| code | HTTP | Meaning |
|---:|---:|---|
| `0` | 200/202 | Success or asynchronous acceptance |
| `400` | 400 | JSON parsing failed or the transport envelope is invalid |
| `40001` | 400 | Invalid parameter |
| `40002` | 413 | Request body exceeds 4 MiB |
| `40101` | 401 | Invalid Token or client credentials |
| `40301` | 403 | Insufficient Scope or authentication-domain mismatch |
| `40401` | 404 | Resource does not exist or is not visible |
| `40402` | 404 | URI is not public |
| `40901` | 409 | Task Revision conflict |
| `40902` | 409 | Idempotency key does not match the original request parameters |
| `40903` | 409 | Operation is not currently cancellable |
| `40904` | 409 | Event cursor has expired |
| `40905` | 409 | Current resource state does not permit the operation |
| `42201` | 422 | Task graph or node configuration validation failed |
| `42202` | 422 | Task runtime or dependency is not ready |
| `42901` | 429 | Rate, queue, or memory budget exceeded |
| `50001` | 500 | Internal device error |
| `50002` | 500 | Service restart interrupted an Operation |
| `50301` | 503 | Dependency service is not ready |
| `52001` | 503 | Media source is unavailable |
| `52002` | 503 | Insufficient device resources |

## 11. Security Requirements

- Production deployments must use trusted HTTPS/WSS certificates and must not disable certificate verification.
- Assign every third-party system an independent `client_id/client_secret` and least-privilege Scopes.
- Never place `client_secret` or an Access Token in a URL, log, or analytics event. WSS and recording Tickets may appear only in the protocol-defined temporary URLs and must be redacted from logs.
- Rotate a leaked `client_secret` immediately. Rotation invalidates the old secret and the client's existing Access Tokens.
- Local web sessions, third-party client credentials, and cloud device identity are three independent authentication domains; credentials are not interchangeable.
- An OpenAPI Token cannot call private local-web URIs or act as CloudWS device identity. Public WSS binds the authenticated subject when an HTTPS Ticket is issued; a message body cannot switch Tokens.
- OpenAPI rate limits, event subscriptions, and asynchronous Operations use independent quotas. OpenAPI overload or a full queue returns `42901` only to that call and must not block local-web login, CloudWS heartbeat, or existing task runtime threads.
- `operation.state.changed` is delivered privately to the third-party client subject fixed when the Operation was created. Clients cannot observe one another's Operations, and wildcard administrative accounts do not passively receive another client's private events.
- Face, upgrade, and retrieval events use field allowlists and must not become channels for file downloads, face images, feature vectors, or retrieval results.
- Face images are sensitive personal information. The integrator must have a lawful basis and control retention and access.

## 12. Version Compatibility

Clients must ignore unknown response fields. `OpenAPI v1` may add optional fields, node types, and new URIs, but it will not change the established meaning of published fields.

Only APIs listed in this version are part of the formal integration boundary. Private device-web URIs not listed here have no compatibility guarantee.

## 13. Current Capability Boundaries

This section prevents customer systems from treating “queryable status,” “realtime change notification,” and “complete data download” as the same capability. Acceptance is governed by this table, delivered Scopes, and actual API responses. `device/capabilities.features[]` is a high-level hint, not a complete API catalog.

| Capability | Current support | Integration method or limitation |
|---|---|---|
| Capability discovery | Partially structured | `device/capabilities` returns execution locations, node types, and high-level Features; this protocol is authoritative for all URIs, Topics, Scopes, and limits |
| Task create/edit/clone/delete/start/stop | Supported | HTTPS/WSS Command; use idempotency keys for writes and Revision for modifications |
| Task runtime state | Supported | `task.state.changed`; read authoritative state with `tasks/runtime` |
| Node state | Partially supported | `node.state.changed` + `tasks/node_metrics`; currently task-runtime aggregation, without true per-plugin FPS, latency, dropped frames, or resource usage |
| Device identity/version/authorization | Supported | `device/get` |
| CPU/NPU/memory/CMM/temperature/storage | On-demand read supported | `device/metrics`; not periodically pushed, so throttle monitoring-page reads as specified in Section 6.6 |
| Customer platform device-online decision | Supported | Use the customer's own WSS, `ping/pong`, and timeout policy; do not substitute the AIBox official cloud-control state |
| AIBox cloud-control-plane connection state | Change notification supported | `device.state.changed`; indicates only the device-to-AIBox cloud control plane |
| Network state | Read-only | `device/network/get`; this version does not change IP, gateway, DNS, or restart interfaces |
| Realtime video/task media outputs | Output discovery supported | `media/outputs` returns registered task outputs such as RTSP; local-web WebRTC signaling is not public, so play RTSP or transcode through a Gateway |
| Realtime alarm notification | Snapshot-style alarm supported | `alarm.created`/`alarm.deleted`; read `alarms/get` after receipt. Current production path emits when an alarm image is persisted |
| Alarm history | Supported | `alarms/list/get/delete` |
| Original alarm-image download | Not published as generic OpenAPI in this version | Internal file paths are never returned; projects requiring secure image Tickets must confirm that capability in the delivery manifest |
| Recording events and history | Supported | `record.created/deleted`, `records/list/get` |
| Recording download | Supported | `records/download_ticket`; short-lived HMAC Ticket and HTTP Range |
| Device logs | Read-only query | `logs/list/get`; no realtime log stream, and high-frequency polling must not simulate one |
| Face-library metadata and writes | Supported | `faces/*`; writes are asynchronous and return an Operation |
| Face-photo reading | Photo content not returned in this version | `photo_available` indicates existence only; no device path, binary image, or feature vector is returned |
| Plugin/node catalog read | Supported | `nodes/catalog/schema`, `plugins/list/health` |
| Third-party plugin install/enable/disable/uninstall | Not public in this version | `plugin.catalog.changed` can report catalog changes made through the local web UI or cloud management plane |
| Intelligent retrieval | Capability-gated | `retrieval/*`; query results are returned only to the requester and are not broadcast as events |
| Online upgrade | Uses the approved upgrade catalog | `upgrades/check/status/apply/cancel`; arbitrary third-party files cannot overwrite the system |
| Device reboot/shutdown/system-time setting | Not public in this version | Do not call private local-web URIs or misuse upgrade APIs to execute system commands |
| Storage mount/format/recording-policy management | No public management API in this version | `device/metrics.storage[]` is read-only; use `records/*` for recording query/download/delete |
| Local users/cloud binding/license writes | Not public in this version | OpenAPI Client, local-web account, and CloudWS device identity remain isolated; `device/get.authorization` is read-only |
| Webhook/MQTT | Not provided by the device-direct edition | Realtime transport is WSS; HTTPS `events/poll` is for disconnect compensation. Gateway mode may expose a separate northbound push protocol |
| Multi-device/cross-tenant centralized management | Outside the device-direct API | Use Device Gateway; a device-local OpenAPI Client credential cannot become cloud multi-device authority |

The delivery party must not fill these gaps with private local-web URIs, device internal paths, or undocumented temporary fields. Add a formal OpenAPI URI/Topic, Scope, error code, and version note when a new capability is required.

## 14. Customer Integration Acceptance Checklist

Acceptance must cover at least the following items. A pass requires retaining `request_id`, HTTP status, business `code`, and necessary device logs while redacting secrets and Tokens.

| Test item | Pass criteria |
|---|---|
| TLS and credential isolation | HTTPS/WSS use a trusted certificate; third-party credentials cannot sign in to the local web UI or act as CloudWS identity |
| Token lifecycle | Obtain and renew Tokens early; after credential rotation/disable, old-Token requests return 401/403 and the customer closes the old WSS |
| HTTPS/WSS basic calls | `session/get` and `device/get` have identical business semantics over both transports; WSS `id` correctly correlates concurrent responses |
| Event reliability | Subscribe to every authorized Topic; cover duplicate events, compensation beyond 100 events, out-of-order merge, 30-minute renewal, reconnect, and `40904` full recovery |
| Task closed loop | Complete node-catalog load, task create/read/update/clone/delete, Revision conflict, start/stop/restart, and Operation terminal state |
| State and alarms | Process task/node/device-state events; read `alarms/get` after a realtime alarm is persisted and correctly handle deletion events |
| Media and recordings | Discover task outputs; verify controlled Ticket expiry and Range download, and synchronize customer cache after deletion |
| Optional domains | Verify face, retrieval, and upgrade functions for granted Scopes; missing Scopes must consistently return `40301` |
| Retry and overload | Same idempotency key does not repeat execution; reusing it with different parameters returns `40902`; 429 obeys `Retry-After` without affecting local web or CloudWS |
| Restart recovery | After device/service restart, reauthenticate, rebuild snapshots and subscriptions, and do not depend on old Tickets, subscription IDs, or expired cursors |
