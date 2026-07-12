<div align="left">
<a href="README_CN.md"><button>中文</button></a>
</div>

# AIBox SDK — Customer Development Guide

<p align="left">
  <a href="https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/"><img alt="Online Docs" src="https://img.shields.io/badge/%F0%9F%93%96%20Online%20Docs-Read%20the%20Docs-2088FF?style=for-the-badge"></a>
  <a href="https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/"><img alt="Docs views in the last 30 days" src="https://raw.githubusercontent.com/JinsonicAi/jinsonic-ai-sdk/analytics/analytics/docs-traffic.svg"></a>
  <img alt="Firmware" src="https://img.shields.io/badge/Firmware-3.10.2-3DDC84?style=flat-square">
  <img alt="Protocol" src="https://img.shields.io/badge/Protocol-V1.0.2-orange?style=flat-square">
  <img alt="Hardware" src="https://img.shields.io/badge/Hardware-AX650N%20%7C%20AX8850%20%7C%20RK-lightgrey?style=flat-square">
</p>

> 📖 **Full documentation is on the online docs site** (full-text search, side navigation, bilingual):
>
> ### 👉 [https://aibox-sdk-docs.readthedocs.io](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/)
>
> This README is a quick landing page. Detailed content (plugin development, alarm protocol, algorithm capabilities, deployment & operations) is maintained on the online docs site.

---

## What Is This

AIBox SDK (`aibox_sdk`) is the customer development kit for the Jinsonic edge-intelligence algorithm platform. Built on a **plugin + DAG pipeline** architecture, it lets algorithm teams integrate algorithms, connect to alarm platforms, and deploy across multiple hardware form factors — without needing to understand low-level chip details.

```
┌─────────────────────────────────────────────────────────────┐
│                      AIBox Runtime                          │
│                                                             │
│  PluginLoader  ──scan──►  .plugin  ──dlopen──►  plugin_init │
│                                                      │      │
│  NodeFactory  ◄──register_node(type, creator)────────┘      │
│       │                                                     │
│       └──create()──► Node ──attach_to()──► Pipeline (DAG)   │
└─────────────────────────────────────────────────────────────┘
```

**Target hardware**

- ① AX650N / AX8850 standalone boards (all-in-one devices)
- ② AX650N / AX8850 compute cards inserted into AX-based hosts (multi-card mix)
- ③ Third-party hosts + AX compute cards (RK / Raspberry Pi 5 / x86)
- ④ RK local devices (RK35xx / RV1126B, with native RK codec, RGA, and RKNN acceleration)

**Audience**

- Algorithm engineers
- Backend engineers
- Delivery / integration engineers

---

## Documentation

Full docs are hosted on the [online docs site](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/). Direct links to each chapter:

| Chapter | Description | Link |
| :-- | :-- | :-- |
| 🚀 Quick Start | Environment setup, one-shot build, on-board deployment & acceptance | [Open](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/en/quickstart/) |
| 📘 SDK Development Guide | Architecture, repo structure, full interface reference | [Open](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/en/sdk-guide/) |
| 🖥️ Web User Manual | Platform operation manual (create tasks, configure, view alarms) | [Open](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/en/user-manual/) |
| 📍 Runtime Location & Deployment | Standalone / compute-card / RK-local deployment | [Open](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/en/runtime-location/) |
| 🔌 Plugin Development | Plugin lifecycle, Node implementation, fast algorithm integration | [Open](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/en/plugin-development/) |
| 🔔 Alarm Linkage | Alarm protocol, message spec, and linkage routing | [Open](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/en/alarm-linkage/) |
| 🧠 Algorithm Capabilities | Built-in algorithm capability matrix | [Open](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/en/algorithm-capabilities/) |
| ⚙️ Configuration Reference | Full plugin configuration reference | [Open](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/en/plugin-config/) |
| 📦 Deployment & Operations | Delivery workflow and operational troubleshooting | [Open](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/en/deployment-ops/) |
| ❓ FAQ | Frequently asked questions and troubleshooting | [Open](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/en/faq/) |

---

## Quick Start

> Full steps (toolchain download, environment variables, acceptance checklist) are in the [Quick Start docs](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/en/quickstart/).

```bash
# 1. Clone the repository
git clone https://github.com/JinsonicAi/jinsonic-ai-sdk.git
cd jinsonic-ai-sdk

# 2. One-shot build (samples + all plugins)
bash build_sdk_sample.sh
```

Build artifacts:

- Sample binaries: `build/example/<demo_name>`
- Plugin packages: `plugins/build_out/*.plugin`

On-board deployment:

```bash
dpkg -i aibox-runtime_*.deb                              # install runtime
cp plugins/build_out/*.plugin /usr/local/aibox/plugins/  # deploy plugins
service aibox restart                                    # restart service
journalctl -u aibox -f                                   # tail live logs
```

**Cross-compilation toolchain download:**

- [Baidu Netdisk](https://pan.baidu.com/s/18CczjjNDnMhM15VDcAJcpQ?pwd=v8me) (code: `v8me`)
- [Google Drive](https://drive.google.com/drive/folders/15cmvIBABTxfgwNvJvhgTI9tMyAiH8vVT?usp=drive_link)

---

## Built-in Algorithm Components

The SDK ships with the following algorithm plugins (`.plugin` packages), selectable directly when creating a task on the web platform — no extra deployment required. See the [Algorithm Capabilities docs](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/en/algorithm-capabilities/) for the full matrix.

| Category | Representative plugins |
| :-- | :-- |
| 🧍 People & Security | `intrusion`, `loitering`, `absence`, `crowd`, `tripwire`, `peopleflow`, `humanattr`, `facedet`, `facerec` |
| 🚗 Vehicle Management | `lprV2` (LPR), `illpark` (illegal parking), `vehicleflow`, `wrongway` |
| 🔥 Safety Production | `firesmoke`, `helmet` |
| 🎭 Behavior Recognition | `fight`, `falldown`, `calling`, `smoking` |
| 🧠 Open-Vocabulary & AI Review | `promptdet` (prompt detection), `llm` (LLM secondary review) |
| 🔌 Input / Output | `netclient`, `netserver`, `hdmi`, `gb28181`, `p2p`, `alarm`, `record`, `osd` |

---

## Repository Structure

```
jinsonic-ai-sdk/
├── include/                    # Public SDK headers
├── example/                    # Demo samples (frame / capture / ivps / npu / node / alg ...)
├── plugins/                    # Built-in plugin projects
│   ├── netclient_plugin/       # Input: network pull (RTSP)
│   ├── persondet_plugin/       # Algorithm: person detection (with region rules)
│   ├── facedet_plugin/         # Algorithm: face detection
│   ├── firedet_plugin/         # Algorithm: fire / smoke detection
│   ├── catdog_plugin/          # Algorithm: pet detection
│   ├── netserver_plugin/       # Output: network push (RTSP Push)
│   └── hdmi_plugin/            # Output: HDMI display
├── thirdpark/                  # Third-party dependencies (jdk / opencv / curl / ...)
├── docs/                       # Online docs site source (MkDocs)
├── build_sdk_sample.sh         # One-shot build (samples + all plugins)
└── CMakeLists.txt
```

---

## Contributing & Support

- 📖 Documentation: [https://aibox-sdk-docs.readthedocs.io](https://aibox-sdk-docs.readthedocs.io/zh-cn/latest/)
- 🐛 Issues: please file at this repo's [Issues](https://github.com/JinsonicAi/jinsonic-ai-sdk/issues)
- 🌐 中文 README: [README_CN.md](README_CN.md)
