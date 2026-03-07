# 插件配置文件说明与完整示例

节点参数表单由设备/插件下发的 **formList** 驱动，网页根据每条配置的 `type` 渲染对应控件。本文档说明所有支持的字段类型及可选属性，并给出完整示例。

---

## 一、formList 单条配置通用约定

- **key**（必填）：字段键名，与任务 config 中存储的键一致。
- **name**（必填）：左侧展示的标题。支持字符串或多语言对象 `{ "zh": "中文", "en": "English" }`。
- **default**：默认值，打开配置弹窗时若当前无值则使用。
- **enabled**：是否启用，默认 `true`；`false` 时该条不展示。

以下按 **type** 分述，未列出的属性对该类型无效。

---

## 二、支持的 type 与属性

### 1. 单行输入：`input`

| 属性 | 类型 | 说明 |
|------|------|------|
| key | string | 必填 |
| name | string \| {zh, en} | 必填 |
| default | string \| number | 默认值 |
| placeholder | string \| {zh, en} | 占位提示 |
| unit | string | 右侧单位，如 `%`、`ms` |
| inputType | `"text"` \| `"number"` \| `"password"` | 不填时根据 default/min/max 推断为 number 或 text |
| buttonType / buttons | 见下方 | 右侧附加按钮（如 RTSP 测试、ONVIF 搜索） |

### 2. 多行文本：`textarea`

| 属性 | 类型 | 说明 |
|------|------|------|
| key | string | 必填 |
| name | string \| {zh, en} | 必填 |
| default | string | 默认文本 |
| placeholder | string \| {zh, en} | 占位提示 |

### 3. 数字输入：`inputNumber`

与 `input` 行为一致，等价于 `type: "input", inputType: "number"`，支持 `min`、`max`、`unit`。

| 属性 | 类型 | 说明 |
|------|------|------|
| key | name | default | placeholder | unit | min | max | 同 input/数字范围 |

### 4. 密码输入：`password`

输入内容以掩码显示，不显示明文。等价于 `type: "input", inputType: "password"`。

| 属性 | 类型 | 说明 |
|------|------|------|
| key | name | default | placeholder | 同 input |

### 5. 下拉选择：`select`

| 属性 | 类型 | 说明 |
|------|------|------|
| key | string | 必填 |
| name | string \| {zh, en} | 必填 |
| default | string \| number | 默认选中项 |
| list | array | 必填，选项列表，每项 `{ "label": "显示名", "value": "值" }`，label 支持 `{zh, en}` |
| buttonType | string | 如 `faceBtn` 时右侧显示「管理」打开人脸库等 |

### 6. 开关：`switch`

| 属性 | 类型 | 说明 |
|------|------|------|
| key | name | default（boolean） | 单开关 |

### 7. 开关 + 多选（选项组）：`switch` + 有 `list`

当 `type === "switch"` 且配置了 `list` 时，渲染为「主开关 + 下方多选芯片」，需配合 `checkboxKey` 或数值型 value（位掩码）使用。

| 属性 | 类型 | 说明 |
|------|------|------|
| checkboxKey | string | 多选结果存储的 key，与 key 不同时 key 为总开关 |
| list | array | 同 select，每项 label/value |

### 8. 滑块：`slider`

| 属性 | 类型 | 说明 |
|------|------|------|
| key | name | default | unit |
| min | number | 最小值，默认 0 |
| max | number | 最大值，默认 100 |

### 9. 只读文本：`readOnly` / `readonly` / `text`

仅展示不可编辑，常用于版本号、只读状态等。

| 属性 | 类型 | 说明 |
|------|------|------|
| key | name | default | unit（可选，显示在值后） |

### 10. 状态标签：`status` / `label` / `tag`

以胶囊徽标形式展示状态或标签，**按值自动配色**：成功（绿）、失败（红）、警告（橙）、中性（灰）。

| 属性 | 类型 | 说明 |
|------|------|------|
| key | name | default | placeholder |

**配色约定**（值不区分大小写）：`OK`/`PASSED`/`REGISTERED`/`RUNNING`/`CONNECTED` → 绿色；`FAILED`/`ERROR`/`TIMEOUT`/`DISCONNECTED` → 红色；`WARNING`/`PENDING`/`UNTESTED` → 橙色；其他 → 灰色。

### 11. 按键：`button`

仅显示一个按钮，无输入框。点击后由前端/设备根据 `buttonType` 或 `key` 执行逻辑（如测试连接、ONVIF 搜索）。

| 属性 | 类型 | 说明 |
|------|------|------|
| key | string | 必填，用于区分不同按钮 |
| name | string \| {zh, en} | 必填，左侧标题 |
| buttonType | string | 必填，如 `testConnection`、`rtspTest`、`onvifSearch`，用于前端/设备识别动作 |
| buttonLabel | string \| {zh, en} | 可选，按钮文案；不填则按 buttonType 映射（如 testConnection →「测试连接」） |
| function | string | 可选，与 buttonType 类似，用于设备端识别 |

### 12. 分段标题：`divider`

不绑定 key，仅作分组标题。

| 属性 | 类型 | 说明 |
|------|------|------|
| name | string \| {zh, en} | 分段标题文案 |

### 13. 时段配置：`schedule`

按时布控，数据结构由前端 ScheduleEditor 约定（含 timezone、schedule 数组等）。

| 属性 | 类型 | 说明 |
|------|------|------|
| key | name | default（如 `{ timezone: "+08:00", schedule: [] }`） |

### 14. 区域绘制：`regionDraw`

打开区域绘制器，存储多边形等区域数据。

| 属性 | 类型 | 说明 |
|------|------|------|
| key | name | default（如 []） | enabled |

---

## 三、多语言

以下字段支持多语言对象，网页根据当前语言选择 `zh` 或 `en`：

- **name**：`{ "zh": "中文", "en": "English" }`
- **placeholder**：同上
- **list[].label**：选项展示名，同上

---

## 四、完整配置示例（formList 片段）

下面是一段包含常用类型的 **formList** 示例，可直接放入插件的 `component.formList` 中参考或裁剪使用。

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
  { "type": "password", "key": "password", "name": { "zh": "密码", "en": "Password" }, "placeholder": "选填" },
  { "type": "textarea", "key": "remark", "name": { "zh": "备注", "en": "Remark" }, "placeholder": "选填" },
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
