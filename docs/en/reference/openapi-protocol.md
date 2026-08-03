# AIBox Third-Party OpenAPI Integration Protocol

| Document Property | Content |
|---|---|
| Document Name | AIBox Third-Party OpenAPI Integration Protocol |
| Document Version | `1.4.3` |
| Protocol Version | `OpenAPI v1` |
| Release Date | `2026-08-03` |
| Target Audience | Third-party platforms, customer-developed Web/server applications |

> **Scope Boundary**: This document only describes how customers connect to devices, send requests, and handle responses. AIBox internal Web interfaces, databases, and plugin files are not part of the open protocol.

> **Transport Boundary**: This is the "Direct Device Connection" edition, applicable to scenarios where customer systems can access the device through LAN, VPN, private network, or private APN. When the device is behind NAT/4G/public Internet, it should proactively establish a WSS long connection to AIBox Device Gateway; customer platforms call the Gateway northbound API and must not expose device port `8099` directly to the public Internet. Device uplink protocol, Gateway API, and this protocol share business URIs and data models but use independent device identity, tenant authorization, and connection sessions.

## Document Navigation

| Reading Goal | Corresponding Sections |
|---|---|
| Complete first connection and authentication | Chapters 1–2 |
| Understand HTTP/WSS message format | Chapters 3–4 |
| Create, edit, start/stop tasks | Chapter 5 |
| Receive task, device, plugin, alarm, and recording events | Chapters 6–7 |
| Query all APIs | Chapter 8 |
| Handle pagination, retry, and errors | Chapters 9–10 |
| Security and version compatibility requirements | Chapters 11–12 |

## 1. Connection Information

| Item | Value |
|---|---|
| HTTPS Address | `https://<Device IP>:8099/openapi/v1/command` |
| WSS Address | `wss://<Device IP>:8099/openapi/v1/ws?ticket=<ticket>` |
| Request Format | UTF-8 JSON |
| HTTP Method | `POST` |
| Authentication Header | `X-Access-Token: <access_token>` |
| Single HTTP/WSS Message Limit | `4 MiB` |
| Access Token Validity | Default `3600` seconds |

All HTTPS business calls are sent to `/openapi/v1/command`. Other `/openapi/v1/...` paths in this document are the `uri` in the request JSON (referred to as **Command URI**), not directly accessible HTTP paths. Historical internal compatibility paths are not part of the third-party protocol. For example:

```http
POST /openapi/v1/command HTTP/1.1
Content-Type: application/json
X-Access-Token: <access_token>

{"uri":"/openapi/v1/device/get","param":{}}
```

### 1.1 Deployment Modes

| Deployment Mode | Connection Direction | Formal Channel | Applicable Scenarios |
|---|---|---|---|
| Direct Device Connection | Customer System → Device | HTTPS + WSS | LAN, VPN, private network, private APN |
| Customer Cloud Management | Device → Device Gateway | WSS uplink; Customer Platform → Gateway HTTPS/WSS/Webhook | NAT, 4G/5G, Internet, and multi-device centralized management |

Customer-developed browsers must not save `client_secret`. Browsers should log in to the customer's own BFF/Gateway, with the server holding third-party credentials. The current Client Credentials flow is for trusted servers, not browser public client authorization flows.

The delivery party will provide for each third-party system:

```text
client_id     Public identifier, e.g., aibc_xxx
client_secret Secret key, e.g., aibsk_xxx, displayed only once
```

`client_secret` is only used to exchange for short-term Access Tokens and must not be placed in URLs, Web front-end code, or regular logs.

Third-party `openapi_client` Access Tokens can only be placed in `X-Access-Token`. URL Query, Cookie, top-level JSON `token`, and `param.access_token` are not OpenAPI authentication methods. Internal legacy Web Token parsing does not constitute an external protocol.

## 2. Complete First Call in Five Minutes

### 2.1 Get Access Token

Request without `X-Access-Token`:

```bash
curl -X POST "https://<Device IP>:8099/openapi/v1/command" \
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

Success response:

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

Failure response:

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

Continuous failures will temporarily lock the client and return HTTP `429`. Clients must wait `retry_after_ms` before retrying.

### 2.2 Verify Session

```bash
curl -X POST "https://<Device IP>:8099/openapi/v1/command" \
  -H "Content-Type: application/json" \
  -H "X-Access-Token: <access_token>" \
  -d '{"uri":"/openapi/v1/session/get","param":{}}'
```

Response:

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

### 2.3 Query Device and Tasks

After successful login, it's recommended to call in sequence:

```text
/openapi/v1/device/get
/openapi/v1/device/capabilities
/openapi/v1/nodes/catalog
/openapi/v1/tasks/list
```

Query tasks example:

```json
{
  "uri": "/openapi/v1/tasks/list",
  "request_id": "req-task-list-001",
  "param": {"page_size": 50}
}
```

Response:

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

!!! note "Full Protocol Reference"
    This document provides a quick overview. For complete API specifications, data models, error codes, event schemas, pagination, retry strategies, security requirements, and version compatibility, please refer to the full Chinese version at [OpenAPI Protocol (Chinese)](../../zh/reference/openapi-protocol.md) or the original document in the `doc/` directory.

## Key Concepts

### Authentication Flow

1. Exchange `client_id` + `client_secret` for Access Token via `/openapi/v1/auth/token`
2. Include Access Token in `X-Access-Token` header for all subsequent requests
3. Tokens expire after 3600 seconds (default); renew before expiration

### Request Format

All requests POST to `/openapi/v1/command` with JSON body:

```json
{
  "uri": "/openapi/v1/<resource>/<action>",
  "param": { /* parameters */ },
  "request_id": "unique-id",
  "idempotency_key": "write-operation-key"
}
```

### Response Format

Success (`code: 0`):

```json
{
  "code": 0,
  "msg": "OK",
  "result": { /* response data */ }
}
```

Failure (`code != 0`):

```json
{
  "code": 40001,
  "msg": "ERROR_CODE",
  "result": {
    "error": {
      "category": "argument|authentication|...",
      "retryable": true|false
    }
  }
}
```

### WebSocket Connection

1. Get ticket via `/openapi/v1/ws/ticket` (HTTPS, with Access Token)
2. Connect to `wss://<IP>:8099/openapi/v1/ws?ticket=<ticket>`
3. Send requests with `type: "request"` and receive `type: "response"`
4. Subscribe to events with `/openapi/v1/events/subscribe`

### Task Lifecycle

1. **Create**: `/openapi/v1/tasks/create` with complete task definition
2. **Modify**: `/openapi/v1/tasks/save` with `expected_revision` (optimistic locking)
3. **Start**: `/openapi/v1/tasks/start` returns async `operation_id`
4. **Monitor**: Poll `/openapi/v1/operations/get` until `state: succeeded|failed`
5. **Stop**: `/openapi/v1/tasks/stop`
6. **Delete**: `/openapi/v1/tasks/delete` with `expected_revision`

### Real-time Events

Subscribe to topics via WSS:

- `task.state.changed` — Task runtime state
- `alarm.created` / `alarm.deleted` — Alarm events
- `record.created` / `record.deleted` — Recording events
- `plugin.catalog.changed` — Plugin installed/updated
- `operation.state.changed` — Async operation progress

Events use "lightweight event + snapshot read" pattern: event notifies change, client calls authoritative API to fetch latest state.

## API Categories

### Session & Operation (8.1)

- `auth/token` — Get Access Token
- `session/get` — Verify current session
- `ws/ticket` — Get WSS connection ticket
- `operations/get|list|cancel` — Manage async operations
- `events/subscribe|poll|ack|unsubscribe` — Event subscription

### Device & Node (8.2)

- `device/get|metrics|capabilities` — Device info and hardware metrics
- `nodes/catalog|schema` — Available nodes and configuration schemas
- `plugins/list` — Installed plugins

### Task Management (8.3)

- `tasks/create|get|list|save|delete` — Task CRUD
- `tasks/start|stop` — Task lifecycle
- `tasks/runtime|node_metrics` — Runtime state

### Media & Recording (8.4)

- `media/outputs` — Get task output streams
- `records/list|download_ticket` — Query and download recordings

### Alarm (8.5)

- `alarms/list|get|delete` — Alarm management

### Configuration (8.6)

- `graphs/validate` — Validate task graph before creation

## Best Practices

1. **Always check both HTTP status and JSON `code`** — Only `code: 0` means success
2. **Use `idempotency_key` for write operations** — Same key = same business operation
3. **Handle optimistic locking** — Always get latest `revision` before `save`
4. **Deduplicate events by `event_id`** — Events are at-least-once delivery
5. **Store subscription `cursor`** — Reconnect and poll missed events
6. **Validate node schemas** — Call `nodes/schema` to get actual supported config
7. **Monitor async operations** — Poll until terminal state (`succeeded|failed|canceled`)
8. **Secure `client_secret`** — Server-side only, never in browser or logs

## Integration Checklist

- [ ] Obtain `client_id` and `client_secret` from delivery team
- [ ] Implement Access Token refresh before expiration
- [ ] Call `device/capabilities` to discover available features
- [ ] Fetch `nodes/catalog` and `nodes/schema` for supported nodes
- [ ] Validate task graphs with `graphs/validate` before creation
- [ ] Handle task `revision` conflicts with optimistic locking
- [ ] Poll async operations after `start|stop` commands
- [ ] Subscribe to events and implement cursor-based polling
- [ ] Test reconnection and missed event recovery
- [ ] Implement idempotency key generation for write operations

---

**For complete documentation**, including:

- Full API reference (64 URIs)
- Detailed data models
- Error code catalog
- Event schemas
- Pagination strategies
- Retry policies
- Security requirements
- Version compatibility

Please refer to the [Chinese version](../../zh/reference/openapi-protocol.md) or contact technical support.
