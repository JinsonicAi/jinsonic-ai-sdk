# Cloud Management Platform User Guide

> Version 1.0 · Updated 2026-09-09 · For device administrators, deployment engineers, monitoring operators, and support teams

[简体中文](../../zh/cloud-platform/index.md) · [Device Web User Manual](../user-manual/index.md) · [Device Onboarding and Quick Start](../device-onboarding.md)

This guide explains how to connect an AI-BOX to the cloud management platform, claim it into your organization, manage its analysis tasks remotely, watch live video, investigate alarm evidence, and maintain the installation. Follow the sequence: prepare the device → configure its cloud connection → add the device → verify connectivity → configure a task → view live video → review alarms.

!!! note "Interface and version"
    This guide reflects the cloud and device interfaces reviewed in September 2026. Screenshots show actual application pages. Device names, counts, alarm statistics, and status values are examples captured at that moment. Firmware, installed plugins, account permissions, and later releases may change the available controls. Both language editions use the same real screenshots; this edition explains their Chinese labels in English and includes a bilingual menu glossary.

Click any screenshot to open the original image and inspect small labels or complete field values.

**Reading paths by role**

- **First-time onboarding:** Sections 2–4 cover preparation, device cloud settings, and one-time claiming.
- **Configuration staff:** Sections 6–8 cover credentials, task setup, and acceptance.
- **Monitoring operators:** Sections 5 and 9–11 cover the dashboard, video, alarms, and audits.
- **Deployment and support:** Sections 13–15 cover the complete example, troubleshooting, and handover.

## 1. Understand the three access points

### 1.1 Device Web, cloud platform, and remote device login

| Access point | Purpose | Account to use | Access requirement |
| --- | --- | --- | --- |
| Device Web interface | Configure local networking, cloud connectivity, plugins, storage, and local tasks | The device's account | Your computer can reach the device network |
| Cloud management platform | View claimed devices, manage tasks remotely, watch video, review alarms, and inspect audit records | Your cloud account | Your computer can reach the cloud; the device must be online for operations requiring it |
| Device login inside the cloud | Verify permission to operate a specific device after cloud login | The device's account | The remote connection to that device is available |

Cloud and device passwords belong to separate account systems. A successful cloud login does not automatically prove that the device login credentials are valid. Knowing a device password also does not prove that the device belongs to your current cloud tenant.

### 1.2 Addresses used in this guide

| Item | Example | How to use it |
| --- | --- | --- |
| Cloud login | [https://cloud.fiveeyes.cc/login](https://cloud.fiveeyes.cc/login) | Sign in with your own cloud account |
| Device Web used for this walkthrough | `https://192.168.1.234:8099/aibox/login` | Local-network example; substitute your device's IP |
| Device connection for this platform | `wss://cloud.fiveeyes.cc/v1/device-control` | Verified in the supplied device configuration screenshot |
| Private deployment connection | The complete deployment-provided `wss://` endpoint | Preserve the correct hostname, port, and path |

**The cloud website URL is not the device connection URL.** Browser login uses an HTTPS page; the device connects to a WSS service. Do not paste `/login?redirect=/dashboard` into the device's Server Address field. Do not infer the service port solely from the website's port.

This public guide does not include real cloud credentials or valid binding codes. Obtain the device password from its administrator or delivery records.

!!! note "Example devices"
    The device-side address example is `192.168.1.234`. The RK3588 already listed in the cloud account is a different claimed device. Cloud screenshots demonstrate operations on the accessible device; they do not establish that the example IP has been claimed.

### 1.3 Recommended first-use sequence

1. Sign in to the device over its local network and confirm that its services are ready.
2. Open **Settings → Cloud Configuration (云端配置)** and inspect the server address and identity.
3. Confirm that the registered device has a Device SN and a valid one-time binding code.
4. Sign in to the cloud and use **Device Management → Add Device** to claim it.
5. Check **Bound** on the device and **Online** in the cloud.
6. Open that device's task page and complete device authentication.
7. Configure, save, and start an analysis task.
8. Open live video and check the actual picture and analysis overlays.
9. Inspect real events and evidence in Alarm Center.

## 2. Prepare for onboarding

### 2.1 Information to collect

| Information | Source | Used for |
| --- | --- | --- |
| Device IP and Web port | Delivery record, network administrator, or discovery tool | Initial Device Web access |
| Device username and password | Device administrator | Local and remote device authentication |
| Cloud account | Existing registration or project administrator | Cloud login |
| Cloud WSS address | Platform deployment team | Device Cloud Configuration |
| Device SN | Device Cloud Configuration → Device Binding | Add Device form |
| One-time binding code | The same device's Cloud Configuration dialog | Claiming within the displayed validity period |
| Video source address and credentials | Camera/NVR administrator | Task input node |
| Analysis requirements | Project design | Plugin, region, thresholds, schedule, and alarm configuration |

### 2.2 Network and browser checks

- Use a currently supported browser such as Chrome or Edge with JavaScript enabled.
- During initial setup, the computer must reach the device IP and Web port. Remote cloud use requires the device to reach the deployed cloud services.
- Check the device IP, gateway, DNS, and time. A cloud website that opens on your computer does not prove the device's own gateway or DNS is correct.
- Live video also requires a working remote media connection. Website access, device online status, and a rendered first video frame are separate checkpoints.
- Validate one stream before increasing the number of tasks or simultaneous viewing windows.
- If a certificate error appears, ask the device or network administrator to verify the address, certificate, and clock. Disabling browser security checks is not a standard onboarding step.

### 2.3 Know which identifier is which

| Identifier | Meaning | What the user does |
| --- | --- | --- |
| Device SN (设备编号 / 设备序列号) | Business identifier used for cloud registration and claiming | Copy from the device into Add Device |
| Device UID (设备唯一标识) | Stable device identity associated with tasks and alarms | Verify the system-generated value |
| P2P UID (P2P 凭证标识) | Identity used by remote connections | Read only; managed by the system |
| One-time binding code | Temporary proof used to claim the device | Enter only in the cloud claim form |
| Device name | Human-readable display name | Choose a meaningful site/function name |

Do not substitute a Device UID, P2P UID, IP address, or license serial number for the Device SN. Similar-looking values are not interchangeable fields.

## 3. Device side: open Cloud Configuration

### 3.1 Sign in to the device

1. Open the device Web address from a computer that can reach it.
2. Enter the device account and password, then click **Login (登录)**.
3. Wait for the task page and connection state to finish loading.
4. Open the gear menu in the upper-right corner.
5. Select **Cloud Configuration (云端配置)**.

[![Cloud Configuration in the device Settings menu](../../assets/cloud-platform/04-device-menu.png)](../../assets/cloud-platform/04-device-menu.png)

*Figure: actual device menu screenshot supplied by the user. Open the upper-right Settings menu. This Cloud Configuration entry is different from System Settings in the cloud sidebar.*

### 3.2 Read the configuration fields

[![Device Cloud Configuration dialog](../../assets/cloud-platform/05-cloud-config.png)](../../assets/cloud-platform/05-cloud-config.png)

*Figure: actual device configuration supplied by the user. The one-time code shown is an expired historical example and cannot be used to claim a device.*

The device is Connected and Authorized, but its Binding Status is Unbound, so cloud claiming is still required. The device-side **Device Number (设备编号)** maps to **Device Serial Number (设备序列号)** in the cloud Add Device form. Copy this value, not the Device UID or P2P UID below it.

| Section or field | Meaning | Recommended action |
| --- | --- | --- |
| Device SN | Identifier used for cloud registration | Copy the complete value when adding the device |
| Binding Status | Whether a tenant owns the device | Expect Unbound before claiming and Bound afterward |
| One-time Binding Code | Current temporary claim code | Use it with this device's SN, not a code from an old screenshot |
| Code Validity | Remaining lifetime of the current code | Wait for a new valid code if it expires |
| Connection Status | Device connection to cloud services | Check this even after claiming succeeds |
| Server Address | WSS service endpoint | Use the deployment-confirmed value |
| Device UID | Stable device identity | Match it against cloud details and support records |
| P2P UID | Remote connection identity | Read only |
| Authorization Status | Whether the remote identity is authorized | Contact deployment/support if unauthorized |
| Reconnect Interval | Delay between reconnection attempts | Seconds; the UI allows 1–60. Normally retain the existing setting |

### 3.3 Set the server address

1. Inspect the current address. If the device is already connected to the correct platform, keep it.
2. For this example platform, use `wss://cloud.fiveeyes.cc/v1/device-control` when configuration is required. For a private deployment, enter the complete endpoint supplied by its deployment team.
3. Check the hostname, port, required path, and accidental spaces. Do not include the website login path.
4. Click **Save (保存)**.
5. Reopen Cloud Configuration and check the resulting connection state.

Two outcomes need different interpretation:

- **Configuration saved and cloud connection restarted** means the settings were saved and reconnection was initiated. Verify the final connection state as well.
- **Configuration saved, but cloud connection not established** means persistence succeeded while connectivity did not. Continue checking the endpoint, network, clock, and identity authorization.

The dialog refreshes read-only state and the binding-code countdown. An edited address is an unsaved draft until Save is clicked; Cancel closes the edit without applying it.

### 3.4 Verify three independent states

| State | What it proves | What it does not prove by itself |
| --- | --- | --- |
| Connected | The device has a cloud connection | That it belongs to your current tenant |
| Bound | The device has an ownership relationship | That it is currently online or its video plays |
| Authorized | The remote identity credential is valid | That a task is running or a camera produces frames |

Acceptance should verify **correct ownership, online connectivity, usable identity, running task, first video frame, and alarm evidence** separately.

## 4. Cloud side: sign in and add a device

### 4.1 Sign in

1. Open the [cloud login page](https://cloud.fiveeyes.cc/login).
2. Check the phone country/region code, for example `+86` for mainland China.
3. Enter your own phone number and cloud password.
4. Click **Login (登录)** and wait for Dashboard.
5. Verify the account identity at the upper right and confirm the intended project/tenant.

The login page includes registration, password recovery, and a language entry. Availability of those flows depends on the deployment and account policy. A device account is not a cloud phone-number account.

The session is associated with the current browser session. If the login page returns after sign-out or a new session, sign in again with the cloud account.

### 4.2 Open Add Device

1. Select **Device Management (设备管理)** in the sidebar.
2. Check **My Devices (我的设备)** to avoid claiming a device that is already present.
3. Click **Add Device (添加设备)** at the upper right.
4. Complete the **Connect New Device (连接新设备)** form.

[![Cloud device list](../../assets/cloud-platform/02-devices.jpg)](../../assets/cloud-platform/02-devices.jpg)

[![Cloud Add Device form](../../assets/cloud-platform/03-add-device.jpg)](../../assets/cloud-platform/03-add-device.jpg)

### 4.3 Complete the form

| Field | Required? | What to enter | Common mistake |
| --- | --- | --- | --- |
| Device SN | Yes | Copy from the device's Cloud Configuration | Entering an IP or Device UID |
| One-time Binding Code | Yes | The current valid code from the same device | Using an expired code or another device's code |
| Device Name | No | A site/function name such as “North Gate — Analysis Box” | Ambiguous names across installations |
| Device Account | Optional, paired with password | The device login username | Entering the cloud phone number |
| Device Password | Optional, paired with account | The device's current password | Entering an old password after a change |

Enter both device credential fields or leave both empty. You can authenticate later when opening remote task management. **A successful claim does not mean the device password has already been verified**; verification may still be required when operating the device.

### 4.4 Submit and confirm success

1. Recheck the Device SN and remaining code validity.
2. Click **Claim Device (认领设备)** and wait for the result.
3. Confirm that the device appears in the list.
4. Check its display name, model, and Device UID.
5. Return to Device Web and verify that Binding Status updates.
6. Once the cloud card shows Online, continue with task and video validation.

If the code expires during entry, obtain a new valid code instead of repeatedly submitting the old one. If the device already has an owner, ask the original tenant administrator to verify ownership and transfer arrangements. Do not attempt to bypass ownership by changing device identifiers.

### 4.5 Already-claimed devices

If the device is already listed under the current tenant, open its details or task page directly. Video failures, invalid device credentials, and offline status should be diagnosed at their respective layers; rebinding is not the first troubleshooting step.

## 5. Dashboard and navigation

[![Cloud Dashboard](../../assets/cloud-platform/01-dashboard.jpg)](../../assets/cloud-platform/01-dashboard.jpg)

*Figure: the dashboard shows device availability, recent alarm trends, and shortcuts. Counts depend on the current account and the time range shown on the page.*

### 5.1 Dashboard indicators

| Area | Purpose | How to use it |
| --- | --- | --- |
| Total Devices | Number of devices visible to this account | Open Device Management |
| Online Devices and rate | Identify offline equipment | Compare with Device Health |
| Alarms in the Last 7 Days | Recent event volume | Review trend and type distribution |
| Pending Alarms | Direct attention to events | Inspect actual states in Alarm Center |
| Alarm Trend | Seven-day changes | Hover or click a date for its count |
| Alarm Type Distribution | Main event categories | Continue to Alarm Center for filters |
| Recent Alarms | Latest events and evidence shortcuts | Open images, payloads, or the full list |
| Device Health | Current online state | Open the relevant device for investigation |

The last-seven-days indicators and cumulative Alarm Center totals can cover different periods. Different counts alone do not prove missing data. The top-bar **Real-time Connection (实时连接)** indicates the cloud event connection, not successful video playback from every device.

### 5.2 Sidebar entries

| Entry | Purpose | Current behavior |
| --- | --- | --- |
| Dashboard (首页总览) | Operational summary | Useful at the start of a shift |
| Live Monitoring (实时监控) | Open the monitoring wall and select device channels | Also accessible through Live Video on a device card |
| Alarm Center (报警中心) | Filter, inspect, and handle events | Images, recordings, and structured payloads |
| Device Management (设备管理) | Claim devices and inspect details | Links to tasks and live video |
| Task Management (任务管理) | Cross-device task overview | Search devices/tasks and open editing |
| Recording Center (录像中心) | Central recording entry | Not available when labeled Coming Soon |
| Cloud Logs (云端日志) | Audit records | Runtime-log access depends on the released interface |
| System Settings (系统设置) | Platform/tenant settings entry | Not available when labeled Coming Soon |

In a narrow window or on a phone, navigation may be collapsed into the upper-left menu button. Open that menu before selecting a module.

Collapse the sidebar when you need more working space; icon hints identify entries. Use breadcrumbs to return to a parent page. Theme changes affect presentation, not the algorithms configured on devices.

## 6. Device management and details

### 6.1 Read a device card

A card shows the name, model, UID, online state, last address, and last-online time. A missing last address can appear as `—`; evaluate it together with online state and details rather than treating that field alone as a fault.

- **Live Video (实时视频)** opens the device's monitoring page and may be unavailable while offline.
- **Device Details (设备详情)** opens identity and resource information.
- **Double-click the card**, or focus it and press Enter, to open its task page.
- **Refresh Devices (刷新设备)** updates the list and status.

### 6.2 Inspect device health

[![Cloud Device Details](../../assets/cloud-platform/06-device-detail.jpg)](../../assets/cloud-platform/06-device-detail.jpg)

*Figure: Live Video, Task Editing, and Login Credentials are available near the top. Version, uptime, and resource information appear below; scroll in narrower windows to inspect the remaining details.*

After checking the device name and UID, inspect the version, uptime, CPU/NPU, memory, disk, and network information actually returned by the device. Hardware and firmware may not expose every metric. A missing value is not a zero-load reading.

Suggested investigation order:

1. Check online state and last-online time.
2. Verify version, uptime, and identity to ensure the correct device is selected.
3. Look for resource pressure in available CPU/NPU, memory, and storage metrics.
4. Correlate these with task state, frame rate, latency, and node health.
5. Use Device Web logs and storage management when further investigation is needed.

### 6.3 Update saved device credentials

The credentials entry in Device Details updates the device login information held by the cloud. Use it after a device password changes, saved credentials fail, or credentials were omitted during claiming.

1. Open the credentials entry for the correct device.
2. Enter the current device username and password.
3. Save, then reopen that device's task management to verify authentication.
4. If authentication still fails, test the same credentials in the local Device Web interface.

This updates the cloud's saved information; **it does not change the device's password**. A save confirmation does not replace a successful remote-device login check.

### 6.4 Unbind a device

Device Details provides an unbind action. Its confirmation states that the device is removed from the current tenant and saved device credentials are cleared.

Before a transfer, confirm the original tenant, destination tenant, device identifier, handover time, and subsequent claiming process. Unbind is not a refresh or reconnect button. Do not assume it deletes local device tasks or historical recordings.

## 7. Task management: from overview to device

[![Cloud Task Management overview](../../assets/cloud-platform/07-tasks.jpg)](../../assets/cloud-platform/07-tasks.jpg)

*Figure: tasks are grouped under devices. Search is at the top; each task row shows its name, ID, state, and editing entry. The example tasks are not running.*

### 7.1 Find a task

1. Open **Task Management (任务管理)**.
2. Enter a name in **Search Devices or Tasks (搜索设备或任务)**.
3. Expand the relevant device.
4. Verify the task name, ID, and running state.
5. Click the task or **Manage Device Tasks (管理设备任务)**.

Tasks can load independently for different devices. One offline, slow, or failed device does not imply all other devices are unavailable. Retry the affected device where appropriate; do not confuse “not loaded yet” with “deleted.”

### 7.2 Remote device authentication

The task page may show preparing, automatic login, connecting, device login, or an error state. It first attempts saved device credentials when available. If none exist or validation fails, enter the device username and password.

Wait for connection and data loading instead of repeatedly clicking Login. Use the new device password if it was changed locally. A message equivalent to “Device logged in, but saving automatic-login credentials failed” means the current device session may work while future automatic login still needs attention.

### 7.3 Device task workspace

[![Device-specific task workspace](../../assets/cloud-platform/08-device-tasks.jpg)](../../assets/cloud-platform/08-device-tasks.jpg)

The workspace shows task cards, task and running counts, connection state, and refresh controls. Check the device identifier in the breadcrumb before creating, editing, or stopping tasks.

Tasks are configured and executed on the real device. Closing a browser preview normally ends that viewing session; it does not stop the device task.

## 8. Create and run your first analysis task

### 8.1 Define the objective

Start with one video input and one algorithm, such as “North Gate — Intrusion.” Decide on the video source, target objects, detection region, working hours, trigger conditions, recording needs, and whether annotated live video is required.

### 8.2 Create a task

1. Open the intended device's task workspace and select New Task.
2. Enter a descriptive scene/function name.
3. Confirm and open the flow editor.
4. Locate input, algorithm, and output nodes in the node list.
5. Add the nodes and connect the data flow.

[![Task flow editor](../../assets/cloud-platform/09-task-editor.jpg)](../../assets/cloud-platform/09-task-editor.jpg)

*Figure: an existing task in the flow editor. The component panel is on the left, nodes and connections are on the canvas, and Save, Run, and execution-location controls are above.*

### 8.3 Understand each node's responsibility

| Node category | Responsibility | Main configuration |
| --- | --- | --- |
| Video input | Read a camera/NVR/video source | Address, credentials, stream, decoding |
| Algorithm | Detect objects or events | Classes, thresholds, regions, timing, plugin-specific fields |
| OSD | Draw boxes, labels, and status text | Class names, confidence, tracking overlays |
| Network output | Supply video for remote preview | Enablement, stream settings, channel |
| Alarm | Turn events into alarms and evidence | Triggers, intervals/deduplication, snapshots, recordings |
| Recording | Store video according to plugin capability | Destination, duration, retention, free space |

A common annotated-video flow is “Video Input → Algorithm → OSD → Network Output.” Connect alarm and recording branches according to the installed plugins' port and data requirements. Available parameters depend on the plugins installed on the actual device.

### 8.4 Configure the video input

1. Open the input node configuration.
2. Enter a valid camera or NVR stream address.
3. Configure credentials and stream selection as required by the source.
4. Choose resolution and frame rate appropriate for device capacity.
5. Save the node and verify connection direction in the flow.

Credentials embedded in stream URLs are sensitive. Do not paste complete password-bearing RTSP URLs into public tickets or screenshots.

### 8.5 Configure the algorithm and region

1. Select the plugin that matches the required event.
2. Read the descriptions in its current configuration interface.
3. Set target classes, confidence, region, and timing as applicable.
4. Draw regions against the correct image and dimensions.
5. Start with explainable settings and adjust one item at a time using real samples.

Lower thresholds can increase false positives; higher thresholds can miss weak targets. Compare evidence under similar scene conditions rather than assuming one direction is always better. See the [Web User Manual](../user-manual/index.md) and [Algorithm Capabilities](../algorithm-capabilities.md) for plugin-specific guidance.

### 8.6 Save, start, and validate

1. Save node settings and the complete task.
2. Return to its card and select Start.
3. Wait for the starting state to complete.
4. Check running state and available frame-rate, latency, and node-health information.
5. Open live preview and verify a continuously updating picture.
6. Check detection boxes, labels, and overlays against the intended behavior.
7. When a real trigger occurs, inspect the associated alarm and evidence.

Saving configuration, running the task, playing video, and producing alarms are separate success criteria. “Running” alone does not prove the input, algorithm, and every output are healthy.

### 8.7 Edit an existing task

Record the task name, device UID, and intended parameter change first. Inspect the current flow, modify only the required fields, and save. Changes may reload a task or interrupt analysis; schedule them according to the project's operating procedures. Closing an editor does not turn unsaved changes into active configuration.

### 8.8 Interpret task states

| State or symptom | What to do |
| --- | --- |
| Starting / Stopping | Wait; avoid repeated clicks |
| Running | Also verify frame rate, picture, and outputs |
| Not Running / Stopped | Start if required and check why it stopped |
| Schedule Paused | Inspect working periods and resume information before assuming a crash |
| Error / Unhealthy | Inspect nodes, video source, plugins, authorization, and resources |
| No updates for a long time | Check device connectivity and refresh task state |

## 9. Live video and multi-window monitoring

[![Cloud Live Monitoring](../../assets/cloud-platform/10-monitor.jpg)](../../assets/cloud-platform/10-monitor.jpg)

*Figure: a four-window monitoring wall with the channel list on the right. The example device is online, but all tasks are stopped, so tiles are empty and channels are disabled. This illustrates the state before playback.*

### 9.1 Start a preview

1. Click **Live Monitoring (实时监控)** in the sidebar, or click **Live Video (实时视频)** on an online device card in Device Management.
2. Expand **Select Channels (选择通道)** on the right.
3. Choose the device and wait for its tasks to load.
4. Click an available task, or drag it to an empty tile.
5. Wait for playback state and the first rendered video frame.

### 9.2 Which tasks are eligible?

The current monitoring wall requires a running task with network output. Stopped tasks and tasks without network output are disabled and may show **No Network Output (无网络输出)**.

Return to the task editor to check the output node, connections, and running state. Repeatedly refreshing the browser does not create a missing network-output branch.

### 9.3 Layout and window controls

| Action | Control | Notes |
| --- | --- | --- |
| Change layout | Single, 4, 9, or 16 windows | Match the screen and available resources |
| Custom layout | Custom row/column controls | Use the supported limits shown in the interface |
| Add a picture | Click a task or drag it to a tile | Requires online/running/network-output conditions |
| Reposition | Drag a tile's handle | Put priority scenes in prominent positions |
| Remove picture | Tile close/remove control | Removes only the preview, not the device task |
| Reconnect | Tile reconnect button | Useful for a single failed preview |
| Audio | Volume button | Cannot enable audio on a stream without audio |
| Picture fullscreen | Individual tile fullscreen | Useful for detail inspection |
| Wall fullscreen | Page fullscreen control | Useful for operator displays; exit with the page/browser control |

A smaller layout may not accommodate all existing windows. After resizing, verify that important channels remain visible. More simultaneous previews increase browser decoding and network load; fluid playback depends on the device, encoding, bandwidth, and viewing computer.

### 9.4 Preview from the task workspace

The device-specific task workspace also provides preview and multi-preview controls. Eligible task cards can be dragged into its preview area, with layouts selected as needed. This workspace is centered on one device; use cloud monitoring for selecting channels across devices.

### 9.5 Playback troubleshooting

- **Online device but no picture:** check running state, network output, media authorization, and video input.
- **Connecting indefinitely:** investigate the remote media connection and network; retry one channel.
- **Black picture despite apparently connected state:** verify that a first frame is actually rendered and inspect input/encoding on the device.
- **Stuttering:** reduce simultaneous windows, validate one stream, then inspect bandwidth and device load.
- **No audio:** verify that the source and output contain audio and that the tile is not muted.
- **Analysis continues after closing preview:** preview and task execution are independent. Stop the task explicitly in Task Management if required.

## 10. Alarm Center: inspect, filter, and handle events

[![Cloud Alarm Center](../../assets/cloud-platform/11-alarms.jpg)](../../assets/cloud-platform/11-alarms.jpg)

*Figure: the Alarm Center presents totals, type distribution, filters and pagination, followed by event rows. “无录像” (No Recording) means that no recording entry is available for that event.*

### 10.1 Read the alarm page

The page combines totals, events needing attention, type distribution, filters, and the alarm list. Rows show event title/type, device, task, time, evidence entries, and reading/handling state.

### 10.2 Apply filters

1. Open **Alarm Center (报警中心)**.
2. Select a device or keep All Devices.
3. Select an alarm type.
4. Select Unread, Read, Handled, False Positive, or all states.
5. Click **Apply Filters (应用筛选)** and wait for the update.
6. Use **Reset (重置)** to clear conditions.

Dropdown changes edit the filter draft; Apply executes the new query. Type-distribution buttons provide quick filtering. If no results appear, clear the conditions to check whether the filters are too narrow.

### 10.3 Pagination

Top and bottom pagination controls let you change page size and browse results. Check the current page, total pages, and applied filters before concluding an event is missing. Historical events may not be on the first page.

### 10.4 Images, recordings, and event payloads

| Evidence entry | Purpose | Important limitation |
| --- | --- | --- |
| Thumbnail/image | Inspect the captured detection scene | Check time, region, boxes, and targets |
| Recording | Play the event-related clip | No Recording means none is available; configuration and retention matter |
| Payload (报文) | Read structured event details | Verify device UID, task ID, type, time, and additional fields |
| Download | Retain available evidence | Appears according to row state and media availability; cannot retrieve nonexistent media |

Some evidence requires connecting to the device that produced it. An alarm record visible in the cloud does not guarantee immediate access to files on an offline device. Device retention policies may also remove historical media.

### 10.5 Reading versus handling states

| State | Meaning | Next action |
| --- | --- | --- |
| Unread | Not yet read | Inspect evidence and assess the event |
| Read | Viewed, but not necessarily resolved | Handle according to site verification |
| Handled | Marked as resolved | Review payload or download evidence if needed |
| False Positive | Classified as an incorrect trigger | Retain a sample for later configuration review |

A row may show **Read** while its handling button shows **Handled** or **False Positive**. These indicate different aspects of the event. Reading is not equivalent to resolving it.

### 10.6 Recommended handling procedure

1. Verify the device, task, time, and event type.
2. Open the image and recording when available.
3. Confirm the real situation and complete the site's response procedure.
4. Click **Handle (处理)** for a verified, resolved event, or **False Positive (误报)** for a verified incorrect trigger.
5. Confirm the updated state and buttons.
6. Download available evidence when archival is required.

Marking a false positive does not automatically modify algorithm thresholds or regions on the device. Review samples and change the task deliberately. Do not mark unverified events handled merely to reduce a pending count.

### 10.7 Use structured payloads for support

The payload dialog displays event fields and provides Copy JSON. For support, identify the device UID, task ID, event time, type, and observed error. Older events may contain only the structured metadata retained when they were created; absence of newer full-payload fields does not invalidate the event.

Review payloads, stream addresses, and captured images before sharing. Do not publish passwords, valid binding codes, or personal evidence unnecessarily.

## 11. Cloud logs and operation tracing

[![Cloud audit log](../../assets/cloud-platform/12-logs.jpg)](../../assets/cloud-platform/12-logs.jpg)

### 11.1 Query audit records

1. Open **Cloud Logs (云端日志)** and select **Audit Log (操作审计)**.
2. Enter an operation, resource, or user keyword when needed.
3. Select the action type and outcome; optionally enter a resource type such as `device` or `alarm`.
4. Set the start and end dates.
5. Click **Apply Filters**.
6. Review time, actor, resource, outcome, and source summaries.
7. Click **Details (详情)** at the end of a row to inspect request identifiers and security details.

In a narrow window, collapse the sidebar and use the horizontal scrollbar to reach the right-hand filter controls and table columns. Use **Clear Conditions (清除条件)** to clear filters, then apply them as required by the page.

### 11.2 Information to include in a support report

| Information | Why it helps |
| --- | --- |
| Operation time and timezone | Align platform and device logs |
| Action and outcome | Identify the failing step |
| Device UID / Task ID | Locate the actual resource |
| Request ID / Trace ID, when shown | Trace the same request across services |
| Exact visible error | Avoid guessing from “it does not work” |
| Steps already attempted | Avoid repeating diagnostics |

If Runtime Logs shows Integration Pending, that function is not yet available to the tenant interface. Raw server logs remain with platform operations; this message does not mean the device produced no logs.

## 12. Permissions, sessions, and feature boundaries

- Device, alarm, and audit visibility is limited by tenant ownership and account permissions.
- If another project account shows a different device list, verify the account/tenant context first.
- Remote-device and cloud authentication are separate; update expired device credentials independently.
- This tenant user guide is not a platform super-administrator manual. Do not assume ordinary accounts can perform manufacturing registration, change tenant ownership, or assign platform permissions.
- Treat entries labeled Coming Soon or Integration Pending according to their actual availability.
- On shared computers, finish by signing out using the upper-right account controls.

## 13. End-to-end example: connect a box and view analysis

The example name is “North Gate — Analysis Box.” Use your own identifiers and credentials.

| Step | Action | Acceptance signal |
| --- | --- | --- |
| 1 | Sign in to Device Web on the local network | Local task page opens |
| 2 | Settings → Cloud Configuration; verify WSS endpoint | Cloud connection is healthy |
| 3 | Read Device SN and binding code | Code is still valid |
| 4 | Cloud Device Management → Add Device | Claim submission succeeds |
| 5 | Name it “North Gate — Analysis Box” | Correct UID appears in the list |
| 6 | Recheck the device-side binding state | Bound is shown |
| 7 | Double-click the device card and authenticate | Device task workspace loads |
| 8 | Create “North Gate — Intrusion” | Flow includes the correct input, algorithm, and outputs |
| 9 | Save and start | Task runs and receives valid frames |
| 10 | Open Live Video and select the task | Live picture and overlays update continuously |
| 11 | Validate an event in an approved, safe test scenario | Alarm matches the correct device and task |
| 12 | Inspect evidence and handle the event | Evidence and updated state are verifiable |

This sequence validates the complete path from onboarding to operational use. Use a project-approved test procedure; never create a dangerous event for a demonstration.

## 14. Troubleshooting reference

| Symptom | Check first | Recommended response |
| --- | --- | --- |
| Device Web does not open | IP, port, network reachability | Verify the address and network before services |
| Cloud login fails | Country code, phone, password, account state | Use cloud credentials, not the device password |
| Server address cannot be saved | `wss://` prefix | Enter the deployment-provided endpoint |
| Saved but not connected | Device gateway, DNS, clock, service port | Restore connectivity before attempting repeated claims |
| Device SN is empty | Registration and cloud connection | Ask the delivery team to verify registration |
| Binding code is absent | Already bound, connection state, expiry | Bound devices do not need a new code; diagnose connectivity if unbound |
| Binding code expired | Current code and countdown | Obtain a fresh valid code |
| Device already belongs to another owner | Original and current tenant | Coordinate with the original administrator |
| Device connected but absent from cloud list | Claim completion and correct tenant | Verify SN, claim result, and account |
| Bound but offline | Power and cloud connectivity | Inspect network and device services |
| Online but device login fails | Device account/password | Validate locally, then update cloud credentials |
| Empty task list | Truly empty or still loading | Inspect load errors and retry |
| Task unhealthy after start | Input, plugins, authorization, resources, connections | Check nodes from input toward outputs |
| Schedule paused | Working periods and device clock | Inspect schedule and resume indication |
| Monitoring task is disabled | Running state and network output | Start the task and configure output |
| Video connecting or black | Media identity, channel, first frame, input | Validate one stream before adding windows |
| Alarm exists without media | Evidence settings, device online state, retention | Check alarm/recording configuration and storage |
| Filters appear unchanged | Apply Filters button | Apply and wait for completion |
| Dashboard and Alarm Center counts differ | Time range, filters, state definitions | Compare like-for-like queries |
| Handle/False Positive cannot be clicked again | Existing final handling state | Inspect state and available download action |
| Runtime Logs is pending | Released feature scope | Ask platform operations for server-side logs |

## 15. Daily checks and handover checklist

### 15.1 At each shift

1. Verify account/tenant context and online-device count.
2. Investigate priority offline devices and resource alerts.
3. Confirm critical tasks are running or intentionally schedule-paused.
4. Sample live video and analysis overlays.
5. Inspect unread and unresolved events using the site response process.
6. Check evidence availability and device storage.
7. Record unresolved issues, timestamps, device UIDs, and task IDs for handover.

### 15.2 New deployment acceptance

- [ ] Cloud and device accounts handed over and independently verified.
- [ ] Device SN, UID, display name, and installation location mapped.
- [ ] Device claimed into the correct tenant.
- [ ] Online status, authorization, and remote-device login validated.
- [ ] Inputs, regions, thresholds, schedules, and outputs match requirements.
- [ ] Live video renders a first frame and continues updating.
- [ ] Alarm images/recordings, payloads, and handling states tested.
- [ ] Device time, storage policy, and network settings checked.
- [ ] Operators know how to query audits and report problems.
- [ ] Unreleased features, version differences, and outstanding issues recorded.

## 16. Bilingual menu glossary

| Chinese interface | English meaning |
| --- | --- |
| 云端配置 | Cloud Configuration |
| 设备编号 / 设备序列号 | Device SN |
| 一次性绑定码 | One-time Binding Code |
| 绑定状态 | Binding Status |
| 连接状态 | Connection Status |
| 已授权 / 未授权 | Authorized / Unauthorized |
| 首页总览 | Dashboard |
| 设备管理 / 我的设备 | Device Management / My Devices |
| 添加设备 / 认领设备 | Add Device / Claim Device |
| 设备详情 | Device Details |
| 任务管理 | Task Management |
| 管理设备任务 | Manage Device Tasks |
| 实时视频 / 实时监控 | Live Video / Live Monitoring |
| 选择通道 | Select Channels |
| 网络输出 | Network Output |
| 报警中心 | Alarm Center |
| 应用筛选 / 重置 | Apply Filters / Reset |
| 未读 / 已读 | Unread / Read |
| 已处理 / 误报 | Handled / False Positive |
| 报文 | Event Payload |
| 云端日志 / 操作审计 | Cloud Logs / Audit Log |
| 即将推出 / 接入中 | Coming Soon / Integration Pending |

For local-device functions, plugin parameters, recordings, and storage settings, continue with the [Web User Manual](../user-manual/index.md). For initial installation and networking, see [Device Onboarding and Quick Start](../device-onboarding.md).
