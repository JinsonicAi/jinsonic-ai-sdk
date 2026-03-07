# 插件配置参考（中文）

> 适用范围：AIBox SDK 插件 `config.json` 中的 `component.component.formList`、`menuShortcuts` 等 UI 配置字段。

---

## 1. 文件结构总览

一个标准插件配置通常包含两层：

1. 插件元信息（打包与加载）
2. 组件 UI 配置（前端渲染）

示例（节选）：

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

说明：
- `component.parentType`：组件分类（输入/算法/输出/其他）。
- `component.type`：节点类型，需与插件注册类型一致。
- `component.component.formList`：配置项定义数组。

---

## 2. formList 通用约定

每个 `formList` 项建议包含：

- `type`：控件类型（必填）
- `key`：配置键（大部分类型必填）
- `name`：显示名（必填，支持多语言）
- `default`：默认值（可选）
- `enabled`：是否展示，默认 `true`

多语言写法：

```json
{ "zh": "中文名称", "en": "English Name" }
```

支持多语言的常见字段：
- `name`
- `placeholder`
- `list[].label`
- `buttonLabel`

---

## 3. 控件类型与字段说明

## 3.1 `input`（单行输入）

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | string | 固定 `input` |
| `key` | string | 配置键 |
| `name` | string/object | 显示名 |
| `default` | string/number | 默认值 |
| `placeholder` | string/object | 占位提示 |
| `unit` | string | 右侧单位（如 `ms`、`%`） |
| `inputType` | string | 可选：`text`/`number`/`password` |
| `buttons` | array | 右侧动作按钮（推荐） |
| `buttonType` | string | 单动作兼容写法 |

`buttons` 示例（RTSP 地址输入框常用）：

```json
"buttons": [
  { "type": "onvifSearch", "fillKey": "rtsp_url" },
  { "type": "rtspTest", "urlKey": "rtsp_url" }
]
```

---

## 3.2 `textarea`（多行文本）

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | string | 固定 `textarea` |
| `key` | string | 配置键 |
| `name` | string/object | 显示名 |
| `default` | string | 默认文本 |
| `placeholder` | string/object | 占位提示 |

---

## 3.3 `inputNumber`（数字输入）

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | string | 固定 `inputNumber` |
| `key` | string | 配置键 |
| `name` | string/object | 显示名 |
| `default` | number/string | 默认值 |
| `min` | number | 最小值 |
| `max` | number | 最大值 |
| `unit` | string | 单位 |

---

## 3.4 `password`（密码输入）

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | string | 固定 `password` |
| `key` | string | 配置键 |
| `name` | string/object | 显示名 |
| `default` | string | 默认值 |
| `placeholder` | string/object | 占位提示 |

---

## 3.5 `select`（下拉选择）

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | string | 固定 `select` |
| `key` | string | 配置键 |
| `name` | string/object | 显示名 |
| `default` | string/number | 默认值 |
| `list` | array | 选项数组，元素含 `label`/`value` |
| `buttonType` | string | 可选，附加动作 |

选项示例：

```json
"list": [
  { "label": { "zh": "本地", "en": "Local" }, "value": "local" },
  { "label": { "zh": "算力卡1", "en": "Card 1" }, "value": "card1" }
]
```

---

## 3.6 `switch`（开关）

### 单开关

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | string | 固定 `switch` |
| `key` | string | 配置键 |
| `name` | string/object | 显示名 |
| `default` | bool | 默认值 |

### 开关 + 多选

当 `switch` 同时包含 `list` 时，表现为“主开关 + 多选项”：

| 字段 | 类型 | 说明 |
|---|---|---|
| `checkboxKey` | string | 多选结果存储键 |
| `list` | array | 选项数组，常用于位掩码配置 |

---

## 3.7 `slider`（滑块）

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | string | 固定 `slider` |
| `key` | string | 配置键 |
| `name` | string/object | 显示名 |
| `default` | number | 默认值 |
| `min` | number | 最小值，默认 0 |
| `max` | number | 最大值，默认 100 |
| `unit` | string | 单位 |

---

## 3.8 `readOnly` / `readonly` / `text`（只读文本）

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | string | 只读控件类型 |
| `key` | string | 配置键 |
| `name` | string/object | 显示名 |
| `default` | string/number | 默认值 |
| `unit` | string | 可选单位 |

---

## 3.9 `status` / `label` / `tag`（状态标签）

用于状态可视化。常见配色约定：
- 绿色：`OK` / `RUNNING` / `CONNECTED`
- 红色：`FAILED` / `ERROR` / `DISCONNECTED`
- 橙色：`WARNING` / `PENDING`
- 灰色：其它

---

## 3.10 `button`（独立按钮）

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | string | 固定 `button` |
| `key` | string | 按钮标识 |
| `name` | string/object | 左侧标题 |
| `buttonType` | string | 动作类型（如 `testConnection`） |
| `buttonLabel` | string/object | 可选按钮文案 |
| `function` | string | 可选设备动作标识 |

---

## 3.11 `divider`（分段标题）

用于分组展示，通常会形成新的视觉分段。

---

## 3.12 `subdivider`（卡片内子分组）

不新建卡片，只在当前卡片内做轻量分隔。

---

## 3.13 `schedule`（按时布控）

用于配置时间段启停策略。常用默认值：

```json
{
  "timezone": "+08:00",
  "schedule": []
}
```

---

## 3.14 `regionDraw`（区域绘制）

用于区域告警场景（多边形区域等）。常见默认值为 `[]`。

---

## 4. `menuShortcuts`（开始菜单快捷入口）

位置：`component.component.menuShortcuts`（与 `formList` 同级）。

作用：在系统菜单中增加插件快捷入口（如“人脸库管理”“录像管理”）。

结构：

```json
"menuShortcuts": [
  { "label": { "zh": "人脸库管理", "en": "Face Library" }, "actionType": "faceLib" },
  { "label": { "zh": "录像管理", "en": "Record Management" }, "actionType": "recordManager" }
]
```

字段说明：
- `label`：菜单显示名称（支持多语言）
- `actionType`：动作类型（前端/设备约定）

---

## 5. 完整示例（可直接复用）

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

## 6. 兼容性建议

1. 新增字段必须提供默认值，避免旧任务加载失败。
2. 不要随意改动既有 `key`，避免历史配置丢失。
3. 多语言字段建议始终提供 `zh/en` 双语键。
4. 与后端有协议耦合的字段（如 `actionType`）要先对齐枚举。
