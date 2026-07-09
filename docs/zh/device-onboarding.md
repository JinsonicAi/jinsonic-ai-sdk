# 设备开箱与快速上手

本章面向**第一次拿到鲸算 AIBOX 盒子**的用户，带你从物理连接走到在 Web 管理平台跑通第一个算法任务：**连接盒子 → 发现设备 IP → 安装 SDK → 登录管理平台 → 创建任务**。

!!! info "适用范围"
    以**鲸算 AX650 盒子**为例，固件版本 `3.10.2`。不同型号/固件的默认网络行为略有差异，本章会分别说明。

```mermaid
flowchart LR
    A[通电 + 接网线] --> B[发现设备 IP]
    B --> C[安装 / 确认 SDK 服务]
    C --> D[浏览器登录管理平台]
    D --> E[创建算法任务]
    E --> F[启动并验证]
```

---

## 1. 物理连接

1. 将盒子接通电源并开机。
2. 用网线将盒子的**网口（LAN）**接入与你电脑同一个局域网的交换机 / 路由器。
3. 确认电脑与盒子处于**同一网段**，后续才能发现并访问设备。

!!! tip "双网口说明"
    部分型号提供两个网口（LAN1 / LAN2），任选其一接入局域网即可。

---

## 2. 获取设备 IP 地址

设备 IP 的获取方式取决于固件的网络配置。

### 2.1 方式一：设备发现工具（推荐，适用于动态 IP 固件）

!!! warning "最新固件为动态 IP（DHCP）"
    最新盒子固件默认通过 **DHCP 自动获取 IP**，没有固定的默认地址。此时**必须使用设备发现工具**来找到盒子的实际 IP。

1. 从工具链网盘下载 **`Discover.exe`** 设备发现工具（Windows）：
    - [百度网盘](https://pan.baidu.com/s/18CczjjNDnMhM15VDcAJcpQ?pwd=v8me)（提取码：`v8me`）
    - [谷歌网盘](https://drive.google.com/drive/folders/15cmvIBABTxfgwNvJvhgTI9tMyAiH8vVT?usp=drive_link)
2. 在与盒子**同一局域网**的 Windows 电脑上双击运行 `Discover.exe`。
3. 工具会自动扫描并列出局域网内的设备。**双击列表中的设备**即可直接打开该盒子的管理页面。

    ![设备发现工具](../assets/deploy-guide/imgs/discover-tool.png)

### 2.2 方式二：已知默认静态 IP（部分旧固件）

部分固件出厂预置了静态 IP。以**鲸算 AX650 盒子**为例，出厂默认地址为：

| 网口 | 默认 IP |
| :-- | :-- |
| LAN1 | `192.168.6.100` |
| LAN2 | `192.168.9.100` |

此时可将电脑 IP 临时设为同网段（如 `192.168.6.x`），直接用浏览器或 SSH 访问该地址。

### 2.3 将网卡改为自动获取 IP（可选）

若希望盒子接入现有网络并自动获取 IP，可 SSH 登录后修改网卡配置：

```bash
# SSH 登录到盒子（以默认静态 IP 为例）
ssh root@192.168.6.100

# 编辑网卡配置
vim /etc/network/interfaces
```

将 `eth0` / `eth1` 配置为 DHCP：

```text
# interfaces(5) file used by ifup(8) and ifdown(8)
# Include files from /etc/network/interfaces.d:
source /etc/network/interfaces.d/*

allow-hotplug eth0
iface eth0 inet dhcp

allow-hotplug eth1
iface eth1 inet dhcp
```

配置 DNS 并验证网络连通：

```bash
echo "nameserver 8.8.8.8" >> /etc/resolv.conf
ping baidu.com
```

改为 DHCP 后，盒子 IP 会由路由器动态分配，请用设备发现工具重新确认地址。

---

## 3. 安装 / 确认 SDK 服务

多数盒子出厂已预装 AIBOX 服务，可直接跳到[第 4 节](#4-登录-web-管理平台)登录。若需手动安装或升级 SDK，按以下步骤操作。

1. 从百度云获取安装包，路径示例：

    ```text
    aibox/软件安装包/ax650x/aibox-ax650n_1.1.3-202604261921_arm64.deb
    ```

2. 将安装包传到盒子上并安装：

    ```bash
    dpkg -i aibox-ax650n_1.1.3-202604261921_arm64.deb
    ```

3. 安装完成后盒子上会新增一个 `aibox` 服务，确认其状态正常：

    ```bash
    service aibox status
    ```

!!! tip "可选：LLM 插件"
    如需使用 SDK 的 LLM 插件（告警二次审核等），可另行安装百度云上的：

    ```text
    aibox/软件安装包/ax650x/aibox-plugin-llm_1.1.2-202605121019_arm64.deb
    ```

---

## 4. 登录 Web 管理平台

1. 通过[设备发现工具](#21-方式一设备发现工具推荐适用于动态-ip-固件)双击设备打开管理页，或在浏览器直接输入设备地址：

    ```text
    https://<设备IP>:8099/aibox/
    ```

2. 使用默认账号密码登录：**账号 `admin` / 密码 `admin`**。

!!! note "完整平台操作手册"
    Web 平台的全部功能（任务管理、流程编辑、报警、人脸库、录像等）详见 [Web 用户使用手册](user-manual/index.md)。

---

## 5. 创建第一个任务（以行人检测为例）

下面以行人检测为例演示 Web 管理平台的使用。目标 pipeline：

```text
RTSP 拉流 → 行人检测 → OSD 叠加 → RTSP 推流
```

### 5.1 新建任务

登录后点击 **添加任务** 以新增一个行人检测任务。

![任务列表](../assets/deploy-guide/imgs/task-list.png)

![添加任务](../assets/deploy-guide/imgs/add-task.png)

### 5.2 拖拽插件

任务通过**拖拽并连接插件**来编排。先把需要的插件拖进编辑界面。

!!! tip "画布操作"
    鼠标滚轮控制视口缩放，按住鼠标中键移动可拖动视口。

![拖拽插件](../assets/deploy-guide/imgs/drag-plugins.png)

### 5.3 连接组件

每个组件都有输入 / 输出端口，按数据流方向连接各组件。

![连接组件](../assets/deploy-guide/imgs/connect-plugins.png)

!!! warning "OSD 叠加的连接"
    OSD 叠加的输入需**同时**连接「网络拉流」和「行人检测」的输出。这是因为行人检测的输出只包含结构化数据（检测框等），不含视频画面，需由网络拉流提供视频流。

### 5.4 配置组件

双击组件即可配置参数。更多组件配置见 [Web 用户使用手册](user-manual/index.md)。

- 双击**网络拉流**组件，配置 RTSP 拉流地址：

    ![配置 RTSP 拉流](../assets/deploy-guide/imgs/config-rtsp-pull.png)

- 双击**网络推流**组件，配置 RTSP 推流地址，并打开「启用 RTSP 推送」开关：

    ![配置 RTSP 推流](../assets/deploy-guide/imgs/config-rtsp-push.png)

### 5.5 保存任务

点击**保存**。

!!! danger "务必手动保存"
    任务的编排和组件的配置**均不会自动保存**，请确保修改后点击保存。

![保存任务](../assets/deploy-guide/imgs/save-task.png)

### 5.6 启动任务

在编辑画面中点击**运行**，或在任务管理页面点击对应任务的**播放键**，即可启动任务。

![运行任务](../assets/deploy-guide/imgs/run-task.png)

!!! success "断电自动恢复"
    任务的启停状态会被 `aibox` 服务记住。例如行人检测任务启动后重启设备，该任务会被 `aibox` 服务**自动拉起**，无需手动重新启动。

---

## 下一步

- 了解更多算法组件能力 → [算法能力](algorithm-capabilities.md)
- 深入 Web 平台操作 → [Web 用户使用手册](user-manual/index.md)
- 交叉编译与插件开发 → [快速开始](quickstart.md) / [插件开发](plugin-development.md)
