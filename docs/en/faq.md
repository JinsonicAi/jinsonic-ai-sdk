# FAQ

This page collects high-frequency issues in development, deployment, and field operations. Locate problems quickly by symptom.

## Plugin Loading

### `undefined symbol: plugin_init`

**Cause**: Missing `extern "C"` or a misspelled function name.

**Fix**: Ensure the plugin entry points are:

```cpp
extern "C" void plugin_init(SDKInterface* sdk) { ... }
extern "C" void plugin_cleanup(SDKInterface* sdk) { ... }
```

### Model load failure (`ax_error_code_init_model_fail`)

Check:

- Whether `model_files[]` in `config.json` matches the file names inside the package.
- Whether the `.plugin` package actually contains the model files.
- Whether the runtime directory has read permission (`ls -la /usr/local/aibox/plugins/`).

### Frontend form does not show

Check:

- Whether `component.parentType` is `algorithm-component` / `input-component` / `output-component`.
- Whether `component.type` **exactly matches** the `type` registered by `register_node()` (case-sensitive).
- Whether each control in `formList` has the three required fields `type` / `key` / `name`.

## Alarm Chain

### Video plays but no alarm

Check:

- Whether the algorithm threshold is too high.
- Whether the region or tripwire is configured correctly.
- Whether the alarm plugin is connected after the algorithm node.
- Whether `alarm_count` is greater than 0.

### Alarm has OSD but the platform receives nothing

Check:

- Whether the alarm push node is enabled.
- Whether the `http_url` of `alarm_plugin` points to the correct backend.
- Whether the composed `alarm_push_uri` URL is reachable (`curl -X POST <url>`).
- Whether the payload contains `did` / `type` / `time`.
- Whether the `bg_data` base64 image is too large (compress under 200KB).
- Whether the alarm cooldown is in effect.

### TTS does not play

Check:

- `tts_enabled=true` (in `alarm_plugin` config).
- Whether `tts_speaker_ip` is reachable.
- Whether the `local_push_msg` returned by `alarm_fn` contains one of `tts_text` / `tts_url` / `tts_queue`.

### RS485 relay does not trigger

Check:

- Whether the `alarm` node enables the RS485 relay.
- Whether `relay_device` maps to a real serial port (`/dev/ttyS1`, `/dev/ttyUSB0`).
- Whether baud rate, slave ID, channel, and coil address match the relay board.
- Whether the kernel has RS485 mode enabled.
- Whether the serial port is occupied by another process.

## Runtime Location

### `rk.local` does not appear in the list

Check whether the device is an RK / RV / Rockchip platform:

```bash
cat /proc/device-tree/compatible
```

Non-RK platforms do not expose local runtime locations and can only use attached AXCL compute cards (`compute_card_N`).

### RK local runtime cannot find libraries

Confirm RK runtime libraries can be loaded:

```bash
ldd /usr/local/aibox/bin/TaskManager | grep -E 'rga|mpp|rknn'
export LD_LIBRARY_PATH=/usr/local/aibox/lib:$LD_LIBRARY_PATH
```

## Build

### Cross compiler not found

Confirm the toolchain is extracted and `PATH` is set:

```bash
export PATH="/home/work/ax/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-linux-gnu/bin:$PATH"
aarch64-none-linux-gnu-g++ --version
```

See [Quick Start › Prepare the cross-compilation environment](quickstart.md#2-prepare-the-cross-compilation-environment).

### Plugin was not packaged

Ensure the plugin directory ends with `_plugin`. `plugins/CMakeLists.txt` discovers subprojects via the `*_plugin` pattern by default.

## Debug Environment Variables

| Variable | Description |
|---|---|
| `AXPLUGIN_KEEP_TMP_SO=1` | Keep the extracted temporary `.so` for GDB debugging |
| `AIBOX_RK_IVPS_DIAG=1` | Enable RK RGA / IVPS diagnostic logs |
| `AIBOX_RKNN_DMA_INPUT=1` | Try the dma-buf binding path for RKNN input |

## Still stuck?

- Full troubleshooting: [SDK Development Guide](sdk-guide.md)
- Alarm chain: [Alarm Linkage](alarm-linkage.md)
- Deployment and operations: [Deployment and Operations](deployment-ops.md)
