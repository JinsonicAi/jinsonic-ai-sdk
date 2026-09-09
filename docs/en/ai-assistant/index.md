# AI Assistant User Guide

[简体中文](../../zh/ai-assistant/index.md)

The AI assistant is a conversational entry point for video tasks in the AI-BOX Web interface. Use it to inspect device capabilities, choose algorithms, create detection tasks, manage existing tasks, and find alarms, logs, and related recordings. It translates a request into a concrete business operation and presents algorithm, camera, task-selection, and confirmation forms when needed.

This guide is intended for device administrators, deployment engineers, and operators. It follows the workflow: open the assistant, check readiness, create a task, manage tasks, review evidence, and troubleshoot.

!!! note "Scope and illustrations"
    This guide reflects the matching frontend and backend implementation reviewed on September 9, 2026. Availability depends on the application, Web assets, algorithm plugins, independent model extension, and hardware combination. Older releases may not have every form or query described here. Always use the algorithms and runtime locations returned by your device.

    Figure 1 shows the English device interface in a supplied screenshot. Other screenshots use actual application components with illustrative data to explain controls and workflows; they do not show a connected device or prove successful execution. Chinese and English illustrations are maintained separately. Data in the illustrative figures are examples; the task name in Figure 1 is retained as displayed on the device.

Click an illustration to open the original image and inspect its controls and text at full size.

## 1. Find and open the assistant

### 1.1 Device Web entry point

1. Open your device's Web management address and sign in.
2. Wait for the device connection in the header to become available.
3. Click **AI**, to the left of the search icon, to open the **AI assistant · video tasks** floating window.
4. If the Web interface was just upgraded and the entry is missing, refresh to load the matching assets. If it remains unavailable, check the application and Web versions.

[![Figure 1: AI assistant entry in the English AI-BOX device interface](../../assets/ai-assistant/en-01-entry.png)](../../assets/ai-assistant/en-01-entry.png)

*Figure 1: AI-BOX device interface in English. The red arrow points to the AI button immediately to the left of the search icon. Click it to open the assistant; use your own device address.*

### 1.2 Floating window controls

The task page remains interactive while the assistant is open. You can inspect a task and read the conversation at the same time.

| Area or action | Purpose | Usage notes |
|---|---|---|
| Title bar | Move the window | Drag it away from task cards you need to inspect |
| Window edges or lower-right corner | Resize | Increase the height for long lists; the input stays at the bottom |
| Collapse | Reduce obstruction | Expand to return to the current conversation |
| Reset position | Restore the default placement | Useful after moving the window or changing screen size |
| Close | Close the assistant view | Does not undo operations already accepted by the device |
| Status row | Read actual model location and state | “Not started” does not necessarily mean the assistant is unavailable |
| Assistant help icon | Expand dependency, mode, and installation information | Normally collapsed when dependencies are available |
| Model service icon | Open shared model settings | Inspect preferences, available locations, and diagnostics |
| Conversation | Read responses, forms, and evidence links | Scroll independently to inspect longer responses |
| Bottom input | Enter a request | Click Send or press Enter; Shift+Enter inserts a line break |

[![Figure 2: Assistant window, status row, and bottom input](../../assets/ai-assistant/en-02-overview.png)](../../assets/ai-assistant/en-02-overview.png)

*Figure 2: Actual application components with example state. The top row contains model status, help, and settings; the bottom contains shortcuts and the input. No device is connected.*

Window geometry is stored in the current browser; another browser may use a different position. With the title bar focused, use arrow keys to move the window. The lower-right resize control also supports arrow keys; Shift increases the step. Compact windows may hide shortcuts without disabling text input.

## 2. Check prerequisites

### 2.1 Preparation checklist

| Item | What to verify | If unavailable |
|---|---|---|
| Device connection | Signed-in session and working device message channel | Restore connectivity and wait for automatic checks |
| Application and Web interface | Compatible versions | Upgrade the matching release components |
| Algorithm plugins | Required algorithms installed, loaded, and visible | Check plugin management, then query the catalog again |
| LLM bridge component | Application-side component available | Follow the Check LLM component prompt |
| Independent model extension | Correct AX/AXCL or RK extension | Have an administrator install the matching package |
| Text capability | A working text-only backend | Use supported explicit commands when semantic understanding is unavailable |
| Storage | External model storage remains mounted and readable | Check TF storage and mounts rather than repeatedly removing media |
| Camera | Device can access a valid RTSP stream | Verify network, port, authentication, and stream path |
| Site settings | Regions, directions, face libraries, and other required details | Complete them in the draft editor when requested |

!!! warning "Model extensions and algorithm packages are different"
    Do not upload an independent `.deb` model extension as a `.plugin` algorithm package. The application-side LLM component is separate from the model program and weights. Install a matching release combination using its package instructions. File presence alone does not establish successful inference.

### 2.2 Dependency checks and retry

The assistant checks dependencies when opened and checks again after reconnection. Missing extensions, incomplete files, unavailable components, or failed checks can disable input and display guidance.

1. Open **Assistant help** and read the specific state and reason.
2. Use **Installation help** for a missing extension or **Check LLM component** for a component problem.
3. After an administrator resolves the issue, click **Check again**.
4. Wait until the input becomes available before querying or creating tasks.

Checking does not start the model. You do not need to create an LLM video task first. “Installed,” “supports text,” and “running” are separate facts.

## 3. Your first five minutes

Begin with queries to understand the returned information before creating a task.

1. Open the assistant and wait for dependency checks.
2. Click **List tasks** and review names, IDs, and states.
3. Click **List algorithms** to see actual device capabilities.
4. Enter `Create a smoking detection task` and choose its algorithm and task runtime.
5. Enter the real camera RTSP address in the dedicated form, review it, and click **Confirm**.
6. Read the result: a draft needing settings, a saved task, or a started task. Follow up if verification has not completed.
7. Open the video preview and check the image, detection overlays, and alarms. Complete site validation before production use.

!!! tip "State the goal clearly"
    Recommended: `Create a smoking detection task on the local device.`

    Too vague: `Set it up for me.`

    Creation, pause, and stop requests can change actual task state. Check the target before sending. Use List algorithms or List tasks when you only want to inspect the device.

## 4. Describe a request

### 4.1 Recommended structure

Use **action + object + scope + required conditions**. For example:

```text
List tasks
Create a smoking and missing safety helmet detection task
Pause task "Warehouse entrance"
Show today's fire alarms
Show error logs for this week
Show recordings related to license plate alarms
```

A fixed command syntax is not required. Recognized explicit commands use the device's business services. Requests needing language understanding use an available text model to produce a controlled proposal, which the device validates. When a proposal card appears, check the proposed action before clicking **Confirm**.

### 4.2 Follow-ups and corrections

During an unsubmitted creation flow, you can add algorithms, placement, review, or output requirements: “Also add fall detection,” “Use the first compute card,” or “Add recording.” Inspect the updated selections and form to verify that previous requirements were retained correctly.

After an alarm query, ask for related recordings. After a task list, identify the tasks to manage. References such as “them” or “these” rely on available context. If the previous scope was incomplete or expired, query again and select explicit targets instead of relying on a guess.

When changing topics or cancelling a pending form, state the new target. A later correction does not automatically roll back an already submitted creation or state change.

### 4.3 Understand the different confirmations

| Confirmation | What to inspect | Meaning |
|---|---|---|
| Operation proposal | Action, target, time, and conditions | Accepts the interpretation; task selection may still follow |
| Algorithm or camera form | Algorithms, runtime, camera address, review, and outputs | Continues creation; complete configurations may be saved and started |
| Task selection or deletion | Actual names, IDs, states, and count | Applies to selected targets; deletion has a separate confirmation |

Cancel ends the pending step. If task configuration or state changes while a confirmation is open, the device may require a fresh selection. Review the current list and continue.

## 5. Inspect and select algorithms

### 5.1 Inspect actual capabilities

Click **List algorithms**, or ask `Which algorithms are available?` The catalog comes from components loaded on the current device. It is not the complete product catalog.

Algorithms are marked as:

- **Defaults available**: eligible for a default-configuration creation attempt. Camera, resources, outputs, and field performance still need checking.
- **Site settings required**: typically needs regions, tripwires, directions, a face library, or prompts. Expect a draft workflow.

### 5.2 Select one or more algorithms

1. Check the required algorithms and inspect the **Selected** count.
2. Expand **Other installed algorithms** if the desired item is not initially visible.
3. Choose an available local device or compute card under **Task runtime**.
4. Review retained LLM review and additional-output requirements.
5. Click **Confirm** to continue, or **Cancel** to stop this step.

[![Figure 3: Algorithm selection and task runtime](../../assets/ai-assistant/en-03-algorithms.png)](../../assets/ai-assistant/en-03-algorithms.png)

*Figure 3: Example algorithm selection. Checked boxes indicate this selection. “Site settings required” means defaults alone are insufficient for deployment. The example list does not represent your installed count.*

If an algorithm is unavailable, check installation, loading, and compatibility first. Typing an unknown algorithm name does not bypass device capability validation.

## 6. Create detection tasks

### 6.1 Enter the camera address

After algorithm selection, the assistant may display the **Camera RTSP address** form. Use this dedicated field to enter the address.

```text
rtsp://192.168.1.100:554/live
```

This demonstrates the format. It does not imply every camera uses `/live`. Obtain the actual path, port, username, and password from the camera configuration. Playback on your computer does not prove reachability from AI-BOX; creation checks connectivity from the device.

1. Enter the complete URL without surrounding spaces.
2. Verify the camera location and channel to avoid processing the wrong view.
3. Check **Task runtime**; it is separate from model-service placement.
4. Click **Confirm** and wait for the device result. Do not submit repeatedly.

[![Figure 4: Camera address and task runtime confirmation](../../assets/ai-assistant/en-04-camera.png)](../../assets/ai-assistant/en-04-camera.png)

*Figure 4: Dedicated camera form. The address is used for the current task and hidden from chat history. Saved task configuration still requires appropriate protection.*

The camera field accepts up to 2,048 characters; the main request input accepts up to 4,096. Prefer concise requests instead of pasting large logs or task JSON as instructions.

### 6.2 Single and combined algorithms

```text
Create a smoking detection task
Create a combined smoking and missing safety helmet task
Create a fire and smoke detection task with LLM review
Create a smoking detection task with recording
```

Combined algorithms normally detect independently on the same video source and aggregate their results for display. Smoking + missing safety helmet does not mean “alarm only when the same person is smoking AND missing a helmet.” Same-object correlation, sequence, time windows, and AND conditions require appropriate business rules. A normal combination does not establish those rules.

The default output template includes network output and alarms. Additional recording, HDMI, or other outputs depend on the actual catalog and request. Storage or destination settings may require a draft. An alarm output node does not establish that relays, TTS, or external reporting are enabled; inspect their configuration separately.

### 6.3 Task runtime versus model location

| Setting | What it controls | Where to inspect |
|---|---|---|
| Task runtime | Available execution placement for this video task | Creation form and task configuration |
| Model-service location | Local device or compute card for shared LLM inference | Model service settings |
| Actual model location | Where the resident instance is really running | Assistant status row and model overview |

A local video task does not necessarily require a local LLM. Selecting a compute card does not establish hardware, plugin, or model validation. Choose a location returned as available by the device and resolve resource or compatibility errors as reported.

### 6.4 Drafts requiring site configuration

When site-specific information is missing, the assistant lists requirements and offers **Open draft to complete settings**.

1. Read the missing items: region, station boundary, tripwire, direction, face library, prompt, or other fields.
2. Open the draft and inspect generated nodes and connections.
3. Complete required fields using actual site conditions. Also check video inputs and outputs.
4. Save the task, then start it and open the preview.
5. Validate trigger conditions and false alarms against site acceptance criteria.

An unsaved draft is not a saved or running task. After a refresh, the camera address may need to be entered again so it is not restored from a sensitive persisted receipt. See the [Web User Manual](../user-manual/index.md) for editor details.

### 6.5 Interpret creation results

| Stage | What it establishes | Next step |
|---|---|---|
| Selection or input required | Request is still being completed | Complete the active form |
| Draft requires settings | An editable draft exists | Configure, save, and start |
| Saved | Device has the task configuration | Verify successful startup |
| Started | Startup was accepted or task is running | Check frames and preview |
| Frame verification passed | Fresh output metrics met the checks | Validate playback, image, algorithms, and alarms |
| Saved but startup failed / verification incomplete | A task may exist without a fully working pipeline | Inspect the original task before creating another |
| Result unknown | Web interface cannot establish the outcome | Recheck the original request and task list |

Automatic frame observation is bounded, currently approximately 30 seconds. It does not promise full algorithm validation within 30 seconds or establish that every algorithm branch passed acceptance.

## 7. Manage existing tasks

### 7.1 Check names, IDs, and states first

Enter `List tasks`. Similar names or the same algorithm can match several tasks. Prefer complete task names and verify the ID in the returned candidates.

[![Figure 5: Task names, IDs, algorithms, and runtime states](../../assets/ai-assistant/en-05-tasks.png)](../../assets/ai-assistant/en-05-tasks.png)

*Figure 5: Example task list. Use current device state for actual operations. A read-only list row does not mean the task has been selected for an operation.*

### 7.2 Start, pause, resume, and stop

| Example | Behavior and considerations |
|---|---|
| `Start task "Warehouse entrance"` | Starts the existing target; does not create a duplicate |
| `Pause task "Warehouse entrance"` | Keeps configuration and releases video resources; does not freeze a frame or preserve tracker memory |
| `Resume paused tasks` | Filters paused tasks; inspect the selected scope |
| `Restore the state before pausing` | Uses saved pre-pause state; tasks previously stopped should remain stopped |
| `Stop task "Warehouse entrance"` | Stops execution while retaining configuration |
| `Start all tasks` | Affects the submitted target set; verify scope and capacity before sending |
| `Pause all tasks` | Pauses video tasks; does not stop the entire device service |

Restoring earlier state requires a supported version and stored state records. If an older task has no pre-pause record, the assistant cannot infer whether it was running. Inspect tasks manually as prompted. Task pause and stop are not interfaces for shutdown, reboot, or stopping remote maintenance services.

### 7.3 Select among multiple matches

[![Figure 6: Select tasks for an operation](../../assets/ai-assistant/en-06-selection.png)](../../assets/ai-assistant/en-06-selection.png)

*Figure 6: Example pause selection. Review the action and task IDs, then select only intended targets.*

1. Read **Action to confirm** and verify start, pause, stop, edit, or delete.
2. Inspect each name, ID, algorithm, and state.
3. Select intended targets. Multiple matches do not imply automatic selection of all tasks.
4. Click **Confirm selection**.
5. Read per-task results. A batch may partially succeed; one success does not establish success for every item.

### 7.4 Edit and delete

Enter `Edit task "Warehouse entrance"`, then click **Open task editor**. The existing editor opens for the original task ID. Edit parameters in its forms; arbitrary natural-language parameter changes are not guaranteed.

For deletion, state the target explicitly, for example `Delete task "Warehouse entrance"`. Select the target when required and inspect the separate deletion confirmation.

[![Figure 7: Separate task-deletion confirmation](../../assets/ai-assistant/en-07-delete.png)](../../assets/ai-assistant/en-07-delete.png)

*Figure 7: Recheck names, IDs, and count before confirming. “Cancel; keep tasks” abandons an unsubmitted deletion.*

Execution stops a running task and releases resources before deleting configuration. Stop or release failure should retain configuration and report failure. This operation does not erase historical alarms or recordings. Retained history does not imply that deleted task configuration has one-click recovery.

## 8. Find alarms and image evidence

### 8.1 Time range and type

Start with **Recent alarms**, or ask:

```text
Show today's alarms
Show fire alarms for this week
What license plate alarms occurred in the last seven days?
Show smoking alarms
```

Supported ranges are **today, this week, and the last 7 days**. “Recent” or an omitted time normally uses the last 7 days; always inspect the displayed boundaries. Yesterday, arbitrary dates, and whole-month requests are not silently replaced with another range.

Statistics use device local time. A week starts Monday at midnight. The last 7 days include today and the preceding six calendar days. An incorrect device clock or timezone affects results; check time settings first.

Types depend on recognized categories and actual records. If ambiguous, choose a returned type candidate. **Other alarm types** are a separate group and are not included in the requested type's total.

### 8.2 Read counts and open evidence

[![Figure 8: Alarm count, filter, time range, and evidence controls](../../assets/ai-assistant/en-08-alarms.png)](../../assets/ai-assistant/en-08-alarms.png)

*Figure 8: Alarm example with 3 illustrative records. “No image” means no image is available for that item; it does not establish that the alarm was invalid.*

1. Verify the type and time boundaries.
2. Read the total, task groups, and currently displayed records.
3. Click **View image** where available and inspect the timestamp, task, and scene.
4. Click **View matching alarm records**, or double-click the relevant response, to open the alarm panel.
5. The panel preserves that query's filters and snapshot. The assistant collapses to make room.
6. Submit a new query to include alarms arriving after the original query.

The total is not the number currently displayed. The conversation normally presents up to 20 recent records; the panel continues through the same query. A database error is not a zero count. Retention policies may remove older records. No records do not prove no event occurred, and record counts are not counts of independent incidents. Assess the evidence accordingly.

## 9. Find related recordings

After an alarm query, ask for related recordings, or enter `Show recordings related to this week's license plate alarms`.

[![Figure 9: Alarm-to-recording correlation](../../assets/ai-assistant/en-09-recordings.png)](../../assets/ai-assistant/en-09-recordings.png)

*Figure 9: Example recording correlation. The interface distinguishes checked alarms, alarms without a matching recording, and accessible clips. The example filename is not actual footage.*

1. Verify the inherited alarm type and time range.
2. Review **Alarms checked** and **No correlated recording** counts.
3. Click an accessible clip to open the **recording manager**.
4. If inaccessible, inspect storage and retention rather than creating another task.

A recording may be absent because recording was disabled, the file is unfinished or removed, or correlation found no matching clip. **Recording correlation unavailable · Not checked** means correlation could not be completed; it does not mean a completed check found no footage. At most 20 clips are shown, with a truncation notice when applicable. The assistant does not fabricate historical footage.

## 10. Inspect and download log excerpts

Ask `Show today's error logs` or `Show recent error logs`.

[![Figure 10: Error logs and redacted-excerpt download](../../assets/ai-assistant/en-10-logs.png)](../../assets/ai-assistant/en-10-logs.png)

*Figure 10: Illustrative error logs. Expand individual entries and download the returned redacted excerpts.*

1. Verify the time range and severity.
2. Expand entries to read timestamp, level, and error text.
3. Click **Download redacted excerpts** to save the returned content.
4. Use device log management when full context is required.

The download contains returned excerpts, not a complete device diagnostics archive. Check for site names, internal addresses, and other project information before sharing. Log queries are not an interface for executing operating-system commands.

## 11. Configure the shared model service

### 11.1 Open settings and inspect state

Open **Model service** from the assistant icon or settings menu. Read actual runtime state at the top before inspecting preferences below.

[![Figure 11: Actual model state, preferences, idle release, and diagnostics](../../assets/ai-assistant/en-11-model.png)](../../assets/ai-assistant/en-11-model.png)

*Figure 11: Model-service example. The overview describes the actual instance; preferences describe saved policy. “Automatic” does not mean the model has started.*

| Setting or state | Meaning |
|---|---|
| Currently running on | Actual model instance location; may be unavailable before startup |
| Automatic | Selects using available capabilities and reuses a compatible instance |
| Local / compute card | Explicit placement; unavailable choices report an error instead of silently moving |
| Release after idle | Integer from 30 to 3,600 seconds; use the device's saved value as the reference |
| Available locations | Extension availability and declared text support |
| Diagnostics | PID, state, consumers, queued work, and in-flight work |
| Save | Persists preferences without immediately starting the model |

### 11.2 Why replies may not start the model

Task queries, algorithm queries, and explicit business commands can be handled directly by device services. A model is not loaded for every reply. Language understanding or video LLM inference creates actual model demand.

| Displayed state | Interpretation | Action |
|---|---|---|
| Not started / on demand | No resident model | Continue supported queries; actual inference will request loading |
| Starting / loading | Model is being prepared | Wait for the current request rather than resubmitting |
| Resident and idle | Loaded without active inference | Reuse on later requests; idle policy may release it |
| Inference / queued | Shared service is processing demand | Wait and inspect consumers or queue if needed |
| Error / unconfirmed state | Available evidence does not establish healthy operation | Refresh and inspect diagnostics, extension, and storage |

The assistant and video LLM nodes share one model service. Opening the window and querying status do not create extra instances. First loading can be much slower than a warm request; duration depends on model, storage, hardware, and resource load.

### 11.3 Change preferences

1. Wait until relevant LLM consumers release the instance and settings become editable.
2. Select an available location and enter the idle-release duration.
3. Click **Save** and verify **Saved**.
4. After the next actual inference, inspect the actual runtime location.

If saving fails or status is stale, refresh before assuming a value in an input field is active. Settings may remain locked while the model is in use or resident. Resolve conflicts between existing nodes' explicit placement and global preferences; repeated saving does not force migration.

## 12. Understand LLM review

LLM review is a processing stage after detection, separate from selecting a detector. Current newly created fire templates can include default review; other algorithms can request it explicitly. Inspect the generated proposal and node settings.

- Check for **LLM review requirement retained** in the creation form.
- Verify the LLM stage exists and upstream algorithms request review.
- Review uses the shared service rather than implying another independent instance.
- If a required LLM component is missing, a task without review does not satisfy the original requirement.
- Under new strict templates, failed, timed-out, or indeterminate review can preserve the original alarm. Receiving an alarm does not prove successful model review.
- Validate positive and negative examples, timeouts, and load. Frames or a resident model do not establish review accuracy.

See [Alarm Linkage](../alarm-linkage.md) and the [Full Plugin Configuration Reference](../reference/plugin-config-full.md) for output and plugin settings.

## 13. Disconnection, refresh, and unknown results

Use this workflow to avoid duplicates and incorrect state assumptions.

1. After a submission disconnects or times out, treat the outcome as unknown.
2. When connected, click **Check original request/result**.
3. Inspect whether the original task was created, changed, or deleted.
4. If the receipt cannot be found, finish manual verification before clicking **Task list checked; release wait**.
5. Only then decide whether another request is necessary.

Rechecking uses the original request; it does not mean executing again. Rewording and resending a creation request can create a new request. Repeated clicks and refreshes are not substitutes for receipt checks.

Closing the window, leaving the page, or cancelling an unsubmitted form does not guarantee cancellation of accepted background work. Chat text is not a reliable long-term operational ledger. Use task state and operation receipts to establish outcomes.

## 14. Chinese and English usage

Chinese pages use the Chinese assistant; English pages use English. Interface guidance, states, and buttons follow the selected language. Original task names, camera addresses, and business content are not arbitrarily translated.

| 中文 | English |
|---|---|
| 查看算法列表 | List algorithms |
| 查看任务列表 | List tasks |
| 查看最近报警 | Recent alarms |
| 创建吸烟检测任务 | Create a smoking detection task |
| 暂停任务“Warehouse entrance” | Pause task “Warehouse entrance” |
| 查询今天的火灾报警 | Show today's fire alarms |
| 查看本周错误日志 | Show error logs for this week |
| 查看车牌报警的关联录像 | Show recordings related to license plate alarms |
| 模型服务 | Model service |
| 确认所选任务 | Confirm selection |
| 查询结果 / 原请求重查 | Check original request/result |

Keep the actual stored task name when referring to a task. Switching interface language does not rename it.

## 15. Troubleshooting

| Symptom | Check first | Resolution |
|---|---|---|
| Assistant entry missing | Application and Web version match | Refresh and verify the delivered release |
| Input disabled | Connection, dependencies, pending request | Restore connection or resolve/recheck the pending operation |
| Extension installation prompt | Platform and matching extension | Follow installation guidance, then Check again |
| Files installed but free-form requests fail | Actual text-only backend | Use explicit commands and verify compatible versions |
| Queries work while model is not started | Direct business-service routing | Normal on-demand behavior; do not start a model just for queries |
| Camera check fails | Device-side network, credentials, path | Fix the cause; check whether a task was saved before retrying |
| Algorithm absent | Installation, loading, visibility | Inspect plugin management; do not guess component names |
| Creation returns a draft | Required site settings | Complete and save the draft in the editor |
| Started task has no picture | Input stream, output, playback | Inspect the original task and preview |
| Previous state cannot be restored | Saved pre-pause state | Review tasks and manually choose the intended scope |
| Deletion incomplete | Version change, stop or release failure | Read per-item errors and refresh targets |
| Zero alarms | Time, type, retention, database state | Do not infer absolute site safety from zero |
| Missing image | Evidence absent or inaccessible | Inspect record and storage; do not create substitute evidence |
| No related recording | Recording, completion, retention, correlation | Inspect recording manager and storage |
| Model placement locked | Consumers, queued work, resident instance | Wait for release before changing preferences |
| Controls disabled after disconnect | Original outcome unconfirmed | Recheck the original request and tasks |
| Old form cannot be confirmed | Expiry, connection or target changes | Submit a fresh request and review its new form |
| Window obscures content | Position or size | Move, resize, collapse, or reset position |

## 16. Site acceptance and handover

Record acceptance against the actual deployment. At minimum, verify:

- [ ] Both language pages expose the entry and readable forms and guidance.
- [ ] Connection, catalog, extension, and runtime choices match the deployment.
- [ ] Camera address corresponds to the correct scene; handover screenshots contain no credentials.
- [ ] Automatic and draft creation meet requirements; task IDs are recorded.
- [ ] Start, pause, resume, stop, and deletion scopes and results are checked.
- [ ] Preview, positive and negative detections, and alarm images meet site criteria.
- [ ] Recording storage, accessibility, and alarm correlation work when used.
- [ ] LLM cold start, placement, reuse, release, and failure behavior are verified when used.
- [ ] Original-request checks resolve disconnections without duplicate creation.
- [ ] Application, Web, plugin, and extension versions and unresolved issues are recorded.

For support, provide event time, actual task ID, request description, interface state, and necessary redacted logs. A report saying only “the assistant did not respond” is insufficient. Do not include camera passwords or complete authentication information.

## 17. Further reading

- [Device Onboarding & Quick Start](../device-onboarding.md): first connection, access, and deployment.
- [Web User Manual](../user-manual/index.md): editor, node settings, and preview.
- [Runtime Location and Deployment](../runtime-location.md): hardware and execution placement.
- [Alarm Linkage](../alarm-linkage.md): alarm outputs and integrations.
- [Deployment and Operations](../deployment-ops.md): rollout, upgrades, and maintenance.
- [FAQ](../faq.md): other device and SDK questions.
