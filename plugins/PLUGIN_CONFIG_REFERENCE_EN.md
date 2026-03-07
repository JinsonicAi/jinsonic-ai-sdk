# Plugin Configuration Reference

The node parameter form is driven by **formList** provided by devices/plugins. The web UI renders controls based on each item's `type`. This document describes all supported field types, optional attributes, and provides a complete example.

---

## 1. General Conventions for formList Items

- **key** (required): Field key name, must match the key stored in the task config.
- **name** (required): Label shown on the left. Supports string or i18n object `{ "zh": "中文", "en": "English" }`.
- **default**: Default value used when opening the config dialog if no current value exists.
- **enabled**: Whether the item is enabled, defaults to `true`; when `false`, the item is hidden.

The following sections describe each **type**. Unlisted attributes are not applicable to that type.

---

## 2. Supported Types and Attributes

### 1. Single-line Input: `input`

| Attribute | Type | Description |
|-----------|------|-------------|
| key | string | Required |
| name | string \| {zh, en} | Required |
| default | string \| number | Default value |
| placeholder | string \| {zh, en} | Placeholder hint |
| unit | string | Unit shown on the right, e.g. `%`, `ms` |
| inputType | `"text"` \| `"number"` \| `"password"` | If omitted, inferred from default/min/max as number or text |
| buttonType / buttons | see below | Right-side buttons (e.g. RTSP test, ONVIF search) |

### 2. Multi-line Text: `textarea`

| Attribute | Type | Description |
|-----------|------|-------------|
| key | string | Required |
| name | string \| {zh, en} | Required |
| default | string | Default text |
| placeholder | string \| {zh, en} | Placeholder hint |

### 3. Number Input: `inputNumber`

Same behavior as `input`, equivalent to `type: "input", inputType: "number"`. Supports `min`, `max`, `unit`.

| Attribute | Type | Description |
|-----------|------|-------------|
| key | name | default | placeholder | unit | min | max | Same as input / numeric range |

### 4. Password Input: `password`

Input is masked. Equivalent to `type: "input", inputType: "password"`.

| Attribute | Type | Description |
|-----------|------|-------------|
| key | name | default | placeholder | Same as input |

### 5. Dropdown Select: `select`

| Attribute | Type | Description |
|-----------|------|-------------|
| key | string | Required |
| name | string \| {zh, en} | Required |
| default | string \| number | Default selected item |
| list | array | Required. Options list, each item `{ "label": "Display name", "value": "value" }`. label supports `{zh, en}` |
| buttonType | string | e.g. `faceBtn` shows a "Manage" button to open face library |

### 6. Switch: `switch`

| Attribute | Type | Description |
|-----------|------|-------------|
| key | name | default (boolean) | Single switch |

### 7. Switch + Multi-select (Option Group): `switch` with `list`

When `type === "switch"` and `list` is configured, renders as "Master switch + multi-select chips below". Use with `checkboxKey` or numeric value (bitmask).

| Attribute | Type | Description |
|-----------|------|-------------|
| checkboxKey | string | Key for storing multi-select result; when different from key, key is the master switch |
| list | array | Same as select, each item label/value |

### 8. Slider: `slider`

| Attribute | Type | Description |
|-----------|------|-------------|
| key | name | default | unit |
| min | number | Minimum, default 0 |
| max | number | Maximum, default 100 |

### 9. Read-only Text: `readOnly` / `readonly` / `text`

Display only, not editable. Common for version numbers, read-only status.

| Attribute | Type | Description |
|-----------|------|-------------|
| key | name | default | unit (optional, shown after value) |

### 10. Status Tag: `status` / `label` / `tag`

Displays status or label as a pill badge. **Auto-colored by value**: Success (green), Failure (red), Warning (orange), Neutral (gray).

| Attribute | Type | Description |
|-----------|------|-------------|
| key | name | default | placeholder |

**Color mapping** (case-insensitive): `OK`/`PASSED`/`REGISTERED`/`RUNNING`/`CONNECTED` → green; `FAILED`/`ERROR`/`TIMEOUT`/`DISCONNECTED` → red; `WARNING`/`PENDING`/`UNTESTED` → orange; others → gray.

### 11. Button: `button`

A standalone button with no input field. On click, frontend/device executes logic based on `buttonType` or `key` (e.g. test connection, ONVIF search).

| Attribute | Type | Description |
|-----------|------|-------------|
| key | string | Required, identifies the button |
| name | string \| {zh, en} | Required, label on the left |
| buttonType | string | Required, e.g. `testConnection`, `rtspTest`, `onvifSearch` for frontend/device to identify action |
| buttonLabel | string \| {zh, en} | Optional, button text; if omitted, mapped by buttonType (e.g. testConnection → "Test Connection") |
| function | string | Optional, similar to buttonType, for device-side identification |

### 12. Section Divider: `divider`

No key binding, used as section title only.

| Attribute | Type | Description |
|-----------|------|-------------|
| name | string \| {zh, en} | Section title text |

### 13. Schedule: `schedule`

Time-based schedule. Data structure defined by frontend ScheduleEditor (includes timezone, schedule array, etc.).

| Attribute | Type | Description |
|-----------|------|-------------|
| key | name | default (e.g. `{ timezone: "+08:00", schedule: [] }`) |

### 14. Region Drawing: `regionDraw`

Opens region drawing tool to store polygon/region data.

| Attribute | Type | Description |
|-----------|------|-------------|
| key | name | default (e.g. []) | enabled |

---

## 3. Internationalization (i18n)

The following fields support i18n objects. The web UI selects `zh` or `en` based on current language:

- **name**: `{ "zh": "中文", "en": "English" }`
- **placeholder**: same
- **list[].label**: option display name, same

---

## 4. Complete Example (formList snippet)

Below is a **formList** example with common types. It can be placed in `component.formList` of a plugin for reference or adaptation.

```json
[
  { "type": "divider", "name": { "zh": "连接与开关", "en": "Connection & Switches" } },
  { "type": "switch", "key": "startupProbe", "name": "startupProbe", "default": true },
  { "type": "switch", "key": "autoReconnect", "name": "autoReconnect", "default": true },
  { "type": "inputNumber", "key": "reconnectMinMs", "name": "reconnectMinMs", "default": 500, "min": 100, "max": 60000, "unit": "ms" },
  { "type": "inputNumber", "key": "reconnectMaxMs", "name": "reconnectMaxMs", "default": 10000, "unit": "ms" },
  { "type": "inputNumber", "key": "probeTimeoutMs", "name": "probeTimeoutMs", "default": 1200, "unit": "ms" },
  { "type": "button", "key": "testConnection", "name": { "zh": "测试连接", "en": "Test Connection" }, "buttonType": "testConnection" },

  { "type": "divider", "name": { "zh": "输入与选择", "en": "Inputs & Select" } },
  { "type": "input", "key": "url", "name": { "zh": "RTSP 地址", "en": "RTSP URL" }, "placeholder": "rtsp://..." },
  { "type": "password", "key": "password", "name": { "zh": "密码", "en": "Password" }, "placeholder": "optional" },
  { "type": "textarea", "key": "remark", "name": { "zh": "备注", "en": "Remark" }, "placeholder": "optional" },
  { "type": "select", "key": "decodeLocation", "name": { "zh": "解码位置", "en": "Decode Location" }, "list": [
    { "label": { "zh": "本地", "en": "Local" }, "value": "local" },
    { "label": { "zh": "计算卡1", "en": "Card 1" }, "value": "card1" }
  ] },
  { "type": "slider", "key": "quality", "name": "quality", "default": 80, "min": 1, "max": 100, "unit": "%" },

  { "type": "divider", "name": { "zh": "只读与状态", "en": "Read-only & Status" } },
  { "type": "readOnly", "key": "version", "name": { "zh": "固件版本", "en": "Firmware" }, "default": "1.0.0" },
  { "type": "status", "key": "runStatus", "name": { "zh": "状态", "en": "Status" }, "default": "运行中" }
]
```
