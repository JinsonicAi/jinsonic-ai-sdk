<div align="left">
<a href="README.md"><button>English</button></a>
</div>

# AIBox SDK — 客户开发指南

<p align="left">
  <a href="https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/"><img alt="在线文档" src="https://img.shields.io/badge/%F0%9F%93%96%20%E5%9C%A8%E7%BA%BF%E6%96%87%E6%A1%A3-Read%20the%20Docs-2088FF?style=for-the-badge"></a>
  <img alt="固件基线" src="https://img.shields.io/badge/%E5%9B%BA%E4%BB%B6%E5%9F%BA%E7%BA%BF-3.10.2-3DDC84?style=flat-square">
  <img alt="协议规范" src="https://img.shields.io/badge/%E5%8D%8F%E8%AE%AE%E8%A7%84%E8%8C%83-V1.0.2-orange?style=flat-square">
  <img alt="硬件" src="https://img.shields.io/badge/%E7%A1%AC%E4%BB%B6-AX650N%20%7C%20AX8850%20%7C%20RK-lightgrey?style=flat-square">
</p>

> 📖 **完整文档请访问在线文档站**（带全文搜索、侧边导航、中英双语）：
>
> ### 👉 [https://aibox-sdk-docs.readthedocs.io](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/)
>
> 本 README 为快速入口页，详细内容（插件开发、报警协议、算法能力、部署交付等）均在在线文档站维护。

---

## 这是什么

AIBox SDK（`aibox_sdk`）是江森自控边缘智能算法平台的客户开发套件，采用 **插件化 + DAG 管线** 架构，让算法团队无需理解底层芯片细节即可快速集成算法、对接报警平台并部署到多种硬件形态。

```
┌─────────────────────────────────────────────────────────────┐
│                      AIBox Runtime                          │
│                                                             │
│  PluginLoader  ──scan──►  .plugin包  ──dlopen──►  plugin_init│
│                                                      │      │
│  NodeFactory  ◄──register_node(type, creator)────────┘      │
│       │                                                     │
│       └──create()──► Node ──attach_to()──► Pipeline (DAG)   │
└─────────────────────────────────────────────────────────────┘
```

**目标硬件**

- ① AX650N / AX8850 独立板卡（独立设备）
- ② AX650N / AX8850 算力卡插入 AX 主机（多卡混用）
- ③ 第三方主机 + AX 算力卡（RK / 树莓派 5 / x86 等）
- ④ RK 本地设备（RK35xx / RV1126B 等，支持 RK 本地编解码、RGA 与 RKNN）

**目标读者**

- 算法工程师
- 平台后端工程师
- 交付实施工程师

---

## 文档导航

完整文档托管在 [在线文档站](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/)，各章节直达链接如下：

| 章节 | 说明 | 在线链接 |
| :-- | :-- | :-- |
| 🚀 快速开始 | 环境准备、一键构建、板端部署与验收 | [打开](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/zh/quickstart/) |
| 📘 SDK 开发指南 | 架构、目录结构、核心接口全解 | [打开](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/zh/sdk-guide/) |
| 🖥️ Web 用户使用手册 | 平台端操作手册（建任务、配置、告警查看） | [打开](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/zh/user-manual/) |
| 📍 运行位置与部署 | 独立板卡 / 算力卡 / RK 本地多形态部署 | [打开](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/zh/runtime-location/) |
| 🔌 插件开发 | 插件生命周期、Node 实现、快速集成算法 | [打开](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/zh/plugin-development/) |
| 🔔 报警联动 | 报警协议、报文规范与联动路由 | [打开](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/zh/alarm-linkage/) |
| 🧠 算法能力 | 内置算法组件能力矩阵 | [打开](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/zh/algorithm-capabilities/) |
| ⚙️ 配置参考 | 插件配置文件完整参考 | [打开](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/zh/plugin-config/) |
| 📦 交付运维 | 交付流程与运维排障 | [打开](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/zh/deployment-ops/) |
| ❓ 常见问题 FAQ | 高频问题与排障 | [打开](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/zh/faq/) |

---

## 快速开始

> 完整步骤（工具链下载、环境变量、验收清单）见 [快速开始文档](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/zh/quickstart/)。

```bash
# 1. 克隆仓库
git clone https://github.com/JinsonicAi/jinsonic-ai-sdk.git
cd jinsonic-ai-sdk

# 2. 一键构建（样例 + 所有插件）
bash build_sdk_sample.sh
```

构建产物：

- 样例程序：`build/example/<demo_name>`
- 插件包：`plugins/build_out/*.plugin`

板端部署：

```bash
dpkg -i aibox-runtime_*.deb                              # 安装运行时
cp plugins/build_out/*.plugin /usr/local/aibox/plugins/  # 部署插件
service aibox restart                                    # 重启服务
journalctl -u aibox -f                                   # 查看实时日志
```

**交叉编译工具链下载：**

- [百度网盘](https://pan.baidu.com/s/18CczjjNDnMhM15VDcAJcpQ?pwd=v8me)（提取码：`v8me`）
- [谷歌网盘](https://drive.google.com/drive/folders/15cmvIBABTxfgwNvJvhgTI9tMyAiH8vVT?usp=drive_link)

---

## 内置算法组件

SDK 出厂内置以下算法插件（`.plugin` 包），可直接在 Web 端创建任务时选用，无需额外部署。完整能力矩阵见 [算法能力文档](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/zh/algorithm-capabilities/)。

| 类别 | 代表插件 |
| :-- | :-- |
| 🧍 人员安防 | `intrusion` 区域入侵、`loitering` 徘徊、`absence` 离岗、`crowd` 人群聚集、`tripwire` 绊线、`peopleflow` 人流统计、`humanattr` 行人属性、`facedet` 人脸检测、`facerec` 人脸识别 |
| 🚗 车辆管理 | `lprV2` 车牌识别、`illpark` 违章停车、`vehicleflow` 车流统计、`wrongway` 车辆逆行 |
| 🔥 安全生产 | `firesmoke` 火灾烟雾、`helmet` 安全帽 |
| 🎭 行为识别 | `fight` 打架、`falldown` 摔倒、`calling` 打电话、`smoking` 吸烟 |
| 🧠 开放词汇与智能审核 | `promptdet` 提示词检测、`llm` LLM 二次审核 |
| 🔌 输入 / 输出 | `netclient` 网络拉流、`netserver` 网络推流、`hdmi` HDMI、`gb28181` 国标、`p2p` P2P 直播、`alarm` 报警推送、`record` 录像、`osd` OSD 叠加 |

---

## 目录结构

```
jinsonic-ai-sdk/
├── include/                    # SDK 公共头文件
├── example/                    # Demo 样例（frame / capture / ivps / npu / node / alg ...）
├── plugins/                    # 内置插件工程
│   ├── netclient_plugin/       # 输入：网络拉流（RTSP）
│   ├── persondet_plugin/       # 算法：行人检测（含区域规则）
│   ├── facedet_plugin/         # 算法：人脸检测
│   ├── firedet_plugin/         # 算法：火灾/烟雾检测
│   ├── catdog_plugin/          # 算法：宠物检测
│   ├── netserver_plugin/       # 输出：网络推流（RTSP Push）
│   └── hdmi_plugin/            # 输出：HDMI 显示
├── thirdpark/                  # 第三方依赖（jdk / opencv / curl / ...）
├── docs/                       # 在线文档站源文件（MkDocs）
├── build_sdk_sample.sh         # 一键构建（样例 + 所有插件）
└── CMakeLists.txt
```

---

## 参与与支持

- 📖 文档：[https://aibox-sdk-docs.readthedocs.io](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/)
- 🐛 问题反馈：请在本仓库 [Issues](https://github.com/JinsonicAi/jinsonic-ai-sdk/issues) 提交
- 🌐 English README：[README.md](README.md)
