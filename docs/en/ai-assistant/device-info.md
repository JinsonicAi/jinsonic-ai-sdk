# Device Information Queries

[简体中文](../../zh/ai-assistant/device-info.md) · [Back to the AI Assistant User Guide](index.md)

With the matching rc3 release, ask the assistant about device versions and resource metrics without knowing API names. Chinese and English requests can ask for a single field or several fields together.

!!! note "Check component compatibility"
    This page uses `2.1.1-202609110020-ai-assistant-rc3` as its baseline. The application, Web assets, and LLM bridge/model extension must be compatible. Publishing documentation does not upgrade devices. If an older version returns generic guidance, verify the installed versions first.

## Supported information

| Information | Example | Interpretation |
|---|---|---|
| Software version | `What software version is installed?` | Application version reported by the device, not a cached Web label |
| Firmware version | `What firmware version is on this box?` | Firmware/SDK version supplied by the version interface; formats differ by platform |
| Device identifier | `Show the device identifier` | Device number supplied by the interface; redact as required before sharing |
| Platform | `Which platform is this device?` | Reported platform, not a guess based on task names |
| Device time | `What time does the device report?` | Device-side time, not the operator's computer clock |
| CPU usage | `Show the host CPU usage` | Percentage for the identified hardware |
| NPU usage | `Show the host NPU usage` | Sampled utilization; model inference itself can increase it |
| CMM | `Show the host CMM usage` | Usage and used/total capacity when supported; otherwise explicitly unavailable |
| Memory | `Just the onboard RAM usage, please` | Percentage and used/total capacity in MiB when supplied |
| Temperature | `What is the temperature of compute card 1?` | Temperature in °C for that hardware, not an automatic safety assessment |

Ask `Which software and firmware versions are installed?` or `Show the host CPU, NPU, and memory usage` for multiple fields. `Show device information` requests the currently integrated fields.

Disk, network configuration, uptime, and other properties are outside this page's current field set. Use device management for those details. This feature does not imply access to every possible device attribute.

## Host, compute cards, and follow-ups

```text
What is the temperature of compute card 1?
And its memory usage?
Now show the host CPU usage instead.
```

The first two queries should retain the same compute-card scope; the third explicitly switches to the host. Expressions such as `compute card 1`, `the first compute card`, `计算卡1`, and `第一张计算卡` refer to hardware returned by the actual device.

Check the hardware label, not just the number. A missing, offline, or unsupported card must not be replaced with host values. If card-specific version information is unavailable, the host's software/firmware version must not be presented as the card's version. Split requests if several hardware scopes cannot be distinguished reliably.

These settings are independent:

- **Query scope** selects which hardware's information to read.
- **Video-task runtime** selects where video detection executes.
- **Shared-model runtime** selects where language understanding/review runs.

Asking for a card's temperature does not move a task or change model-service preferences.

## Data sources and freshness

The model interprets the question and selects fields. Device business interfaces supply the values: the version interface supplies versions, and the device-information interface supplies resources. CPU, temperature, and version values are not generated from model guesses.

| Display | Meaning | Action |
|---|---|---|
| Valid `0%` | The interface reported a valid zero | Do not confuse it with a missing field |
| Unavailable / missing | Hardware, field, or interface has no usable data | Check hardware and versions; do not substitute zero |
| Recent sample | A supplied sample timestamp meets current freshness criteria | Also verify hardware and units |
| Stale data | Sample age exceeds current freshness criteria | Query again or inspect sampling |
| Unknown sample time | A legacy interface lacks a valid timestamp | Do not describe it as a freshly sampled real-time value |

Query time and sample time are different: receiving a new reply does not prove a new underlying sample was taken. Version information generally does not use resource-sample expiry rules.

This is an on-demand query, not continuous monitoring. Ask again for another response. Sampling cadence, concurrent workload, and the assistant's own inference may change adjacent readings. Without vendor thresholds and site evidence, a temperature or usage value alone cannot establish absolute safety or a hardware fault.

## Chinese and English examples

| 中文 | English |
|---|---|
| 软件和固件分别是什么版本？ | Which software and firmware versions are installed? |
| 主板CPU占用多少？ | Show the host CPU usage. |
| 本机NPU和CMM占用各是多少？ | Show the host NPU and CMM usage. |
| 一号计算卡有多烫？ | What is the temperature of compute card 1? |
| 那内存呢？ | And its memory usage? |
| 只看本机内存 | Just the onboard RAM usage, please. |

There is no need to translate metric names into algorithm names or create a video task before querying.

## Troubleshooting and site verification

1. Verify a signed-in page and a working device connection.
2. Check compatible application, Web, and model-component versions, including cached assets after upgrades.
3. Confirm host/card scope and whether an expired conversation supplied the wrong reference.
4. Inspect units, sample times, and unavailable markers. Compare with device management for the same scope and a nearby sample time when necessary.
5. Wait for recovery during model loading, busy periods, or service interruption. Do not create duplicate tasks, delete model files, or interpret an error response as a metric.

For support, provide versions, the original question, hardware scope, query time, and a redacted response. Do not supply camera passwords, tokens, or complete authentication data.
