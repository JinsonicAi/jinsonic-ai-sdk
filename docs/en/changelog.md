# Changelog

This page records the main changes of the documentation site and the SDK.

!!! note "Version baseline"
    This documentation corresponds to SDK `aibox_sdk`, firmware baseline `3.10.2`, and platform protocol spec `V1.0.2` (2026-02-21).

## Documentation Site

### 2026-09-06 (OpenAPI documentation and examples 1.4.9)

- Updated the [protocol](reference/openapi-protocol.md) with web-account Client provisioning, preserving existing Client Credentials / Token / business APIs.
- Added Python / Node.js / curl ZIP downloads and SHA-256 to the protocol, with step-by-step instructions included in the package.
- Documented password revocation, one-time Secrets, bounded WSS retries and certificate requirements; the protocol is the single OpenAPI navigation entry.

### 2026-07 (content optimization)

- Homepage redesign: role-based card navigation, architecture diagram, and doc map.
- Quick Start: added toolchain download addresses, step-by-step build/deploy commands, artifact paths, and layered acceptance.
- Runtime Location: added selection decision diagram, RK local runtime environment, and resource isolation rules.
- Plugin Development: added the standard 10-step from-scratch guide, node lifecycle sequence diagram, and config control table.
- Alarm Linkage: added the two-layer protocol, full `alarm_fn` payload, EventType mapping, and a joint-debugging checklist.
- Algorithm Capabilities: added the full algorithm matrix, region rule engine, and industry solution combinations.
- Deployment and Operations: added monitoring metrics detail and debug environment variables.
- Added the [FAQ](faq.md) page.

## SDK Capabilities

### Multi runtime location and RK local support

- Unified Runtime Location abstraction: `ax.local` / `rk.local` / `compute_card_N`.
- RK local supports MPP codec, RGA image processing, and RKNN inference.
- Plugins resolve the runtime location uniformly via `PluginRuntime::from_task_config()` without hardcoding platform branches.

### Open-vocabulary prompt detection

- Added the `promptdet` plugin, defining targets of interest via English prompts.
- Up to 5 prompts per task, with region rules, multi-frame confirmation, and alarm governance.
- Model, text encoder, and vocabulary are packaged into `promptdet_model.axpkg`.

### Alarm protocol

- Extended to `EventType=20` (non-motor-vehicle detection).
- Backends are advised to keep forward compatibility by using the `type + alarm_type` field combination.

## Maintenance Conventions

- When the protocol or user manual has a new version, update the [Source Downloads](reference/downloads.md) attachments and online docs accordingly.
- New fields must have default values; avoid renaming existing fields to prevent database migration.
- Field changes affecting backend parsing must bump the protocol version.
