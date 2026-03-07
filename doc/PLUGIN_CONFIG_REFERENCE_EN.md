# Plugin Configuration Reference (English)

> Scope: UI-related plugin config fields in AIBox SDK `config.json`, mainly `component.component.formList` and `menuShortcuts`.

---

## 1. File Structure Overview

A standard plugin config usually has two layers:

1. Plugin metadata (packaging/loading)
2. Component UI schema (frontend rendering)

Example (excerpt):

```json
{
  "name": "firedet_plugin",
  "version": "3.0.2",
  "platform": {
    "arch": "aarch64",
    "os": "Linux",
    "chip": "ax650n"
  },
  "entry": "libfiredet_plugin.so",
  "md5": "...",
  "type": "firedet_plugin",
  "model_path": "./models",
  "model_files": [
    "models/fs_20241231_npu1.model"
  ],
  "component": {
    "parentType": "algorithm-component",
    "label": { "zh": "火灾检测", "en": "Fire Detection" },
    "type": "firedet_plugin",
    "component": {
      "formList": []
    }
  }
}
```

Notes:
- `component.parentType`: group category (input/algorithm/output/other).
- `component.type`: node type, must match the registered node type.
- `component.component.formList`: array of UI config items.

---

## 2. Common Conventions for `formList`

Each `formList` item should generally contain:

- `type`: control type (required)
- `key`: config key (required for most control types)
- `name`: display name (required, i18n supported)
- `default`: default value (optional)
- `enabled`: visibility flag, default `true`

I18n format:

```json
{ "zh": "中文名称", "en": "English Name" }
```

Fields commonly supporting i18n:
- `name`
- `placeholder`
- `list[].label`
- `buttonLabel`

---

## 3. Control Types and Attributes

## 3.1 `input` (single-line input)

| Field | Type | Description |
|---|---|---|
| `type` | string | fixed as `input` |
| `key` | string | config key |
| `name` | string/object | display name |
| `default` | string/number | default value |
| `placeholder` | string/object | placeholder text |
| `unit` | string | right-side unit, e.g. `ms`, `%` |
| `inputType` | string | optional: `text` / `number` / `password` |
| `buttons` | array | right-side action buttons (recommended) |
| `buttonType` | string | single-action compatible form |

`buttons` example (common for RTSP input):

```json
"buttons": [
  { "type": "onvifSearch", "fillKey": "rtsp_url" },
  { "type": "rtspTest", "urlKey": "rtsp_url" }
]
```

---

## 3.2 `textarea` (multi-line text)

| Field | Type | Description |
|---|---|---|
| `type` | string | fixed as `textarea` |
| `key` | string | config key |
| `name` | string/object | display name |
| `default` | string | default text |
| `placeholder` | string/object | placeholder text |

---

## 3.3 `inputNumber` (numeric input)

| Field | Type | Description |
|---|---|---|
| `type` | string | fixed as `inputNumber` |
| `key` | string | config key |
| `name` | string/object | display name |
| `default` | number/string | default value |
| `min` | number | minimum value |
| `max` | number | maximum value |
| `unit` | string | unit |

---

## 3.4 `password` (password input)

| Field | Type | Description |
|---|---|---|
| `type` | string | fixed as `password` |
| `key` | string | config key |
| `name` | string/object | display name |
| `default` | string | default value |
| `placeholder` | string/object | placeholder text |

---

## 3.5 `select` (dropdown)

| Field | Type | Description |
|---|---|---|
| `type` | string | fixed as `select` |
| `key` | string | config key |
| `name` | string/object | display name |
| `default` | string/number | default selection |
| `list` | array | options; each has `label` and `value` |
| `buttonType` | string | optional side action |

Option example:

```json
"list": [
  { "label": { "zh": "本地", "en": "Local" }, "value": "local" },
  { "label": { "zh": "算力卡1", "en": "Card 1" }, "value": "card1" }
]
```

---

## 3.6 `switch` (toggle)

### Single switch

| Field | Type | Description |
|---|---|---|
| `type` | string | fixed as `switch` |
| `key` | string | config key |
| `name` | string/object | display name |
| `default` | bool | default value |

### Switch + Multi-select

When `switch` includes `list`, it is rendered as “master switch + multi options”:

| Field | Type | Description |
|---|---|---|
| `checkboxKey` | string | key that stores multi-select result |
| `list` | array | options array, often used for bitmask settings |

---

## 3.7 `slider`

| Field | Type | Description |
|---|---|---|
| `type` | string | fixed as `slider` |
| `key` | string | config key |
| `name` | string/object | display name |
| `default` | number | default value |
| `min` | number | minimum, default 0 |
| `max` | number | maximum, default 100 |
| `unit` | string | unit |

---

## 3.8 `readOnly` / `readonly` / `text` (read-only)

| Field | Type | Description |
|---|---|---|
| `type` | string | read-only control type |
| `key` | string | config key |
| `name` | string/object | display name |
| `default` | string/number | default value |
| `unit` | string | optional unit |

---

## 3.9 `status` / `label` / `tag` (status badge)

Used for status visualization. Typical color convention:
- green: `OK` / `RUNNING` / `CONNECTED`
- red: `FAILED` / `ERROR` / `DISCONNECTED`
- orange: `WARNING` / `PENDING`
- gray: others

---

## 3.10 `button` (standalone action button)

| Field | Type | Description |
|---|---|---|
| `type` | string | fixed as `button` |
| `key` | string | button identifier |
| `name` | string/object | left title |
| `buttonType` | string | action type, e.g. `testConnection` |
| `buttonLabel` | string/object | optional button text |
| `function` | string | optional device-side action key |

---

## 3.11 `divider` (section header)

Used for visual grouping; usually starts a new section block.

---

## 3.12 `subdivider` (sub-section inside current card)

Does not create a new card; adds a lightweight separator in the current card.

---

## 3.13 `schedule` (time schedule)

Used for scheduled enable/disable strategies. Common default:

```json
{
  "timezone": "+08:00",
  "schedule": []
}
```

---

## 3.14 `regionDraw` (region drawing)

Used for region-based alarm scenarios (polygons, etc.). Common default is `[]`.

---

## 4. `menuShortcuts` (top menu shortcuts)

Location: `component.component.menuShortcuts` (same level as `formList`).

Purpose: add plugin-specific quick entries in system menu (e.g., “Face Library”, “Record Management”).

Structure:

```json
"menuShortcuts": [
  { "label": { "zh": "人脸库管理", "en": "Face Library" }, "actionType": "faceLib" },
  { "label": { "zh": "录像管理", "en": "Record Management" }, "actionType": "recordManager" }
]
```

Field description:
- `label`: menu display text (i18n supported)
- `actionType`: action key agreed by frontend/device

---

## 5. Full Reusable Example

```json
{
  "component": {
    "parentType": "input-component",
    "label": { "zh": "网络拉流", "en": "Network Pull Stream" },
    "type": "netclient_plugin",
    "component": {
      "formList": [
        {
          "type": "input",
          "name": { "zh": "RTSP地址", "en": "RTSP Address" },
          "key": "rtsp_url",
          "default": "",
          "placeholder": "rtsp://...",
          "buttons": [
            { "type": "onvifSearch", "fillKey": "rtsp_url" },
            { "type": "rtspTest", "urlKey": "rtsp_url" }
          ]
        },
        {
          "type": "divider",
          "name": { "zh": "按时布控", "en": "Scheduled Deployment" }
        },
        {
          "type": "schedule",
          "key": "schedule_config",
          "name": { "zh": "布控时段", "en": "Schedule" },
          "enabled": true,
          "default": {
            "timezone": "+08:00",
            "schedule": []
          }
        }
      ],
      "menuShortcuts": [
        { "label": { "zh": "录像管理", "en": "Record Management" }, "actionType": "recordManager" }
      ]
    }
  }
}
```

---

## 6. Compatibility Recommendations

1. Always provide default values for newly added fields.
2. Avoid changing existing `key` names to prevent old-task config loss.
3. For i18n fields, provide both `zh` and `en` keys.
4. For backend-coupled fields (e.g., `actionType`), align enum definitions first.
