# OpenAPI Example Validation and Limitations

Date: **2026-09-06**. Examples/documentation **1.4.9**, protocol **OpenAPI v1**. This records the checks performed, not certification of every product feature or customer environment.

## Versions and results

| Validation layer | Result | Scope |
|---|---|---|
| Python/Node example regression | 56 passed: Python 35, Node 21 | Isolated HTTPS/WSS fixtures, authentication, permissions, requests, errors and recovery |
| Extracted ZIP entry points | 12 passed | Three provisioning and three connection entries, two task entries, HTTP/WSS events, two download entries |
| Packaging gates | 58 suites passed per platform | Mixed native behavior, race, compile, source-contract and build checks; not 58 device tests |
| AX650N device | 37/37 passed | Delivered ZIP code against a fully installed application package |
| RK3588 device | 37/37 passed | Delivered ZIP code against a fully installed application package |
| Installed critical runtime files | 11/11 hashes matched per device | TaskManager, signaling, companion runtime libraries and related files |

Both devices ran application package `2.0.1-202609062127-openapi-rc4`; example version 1.4.9 is not an application version. Isolated tests used Python 3.14.4/Node.js 22.22.1. The legacy-certificate device comparison used Python 3.12.8/Node.js 22.22.1. A full Python/OS version matrix was not tested.

## What ran on real devices

- Python, Node and curl provisioned Client credentials using a device web account, then used the original `auth/token` and business APIs.
- Invalid inputs, read-only permissions, private-interface isolation, one-time Secret return, rate limiting, and 40 sequential queries reusing a Token on each device.
- A dedicated test account's password change invalidated its old Secret, Token and established WSS. Restoring the old password did not revive credentials. Permission changes and account deletion also revoked them.
- An isolated real H.264 RTSP source supported creating, editing, starting, checking media outputs, stopping and deleting new test tasks; metrics, idempotent writes and revision conflicts were exercised.
- HTTP event consumption/resumption and WSS ready, real task events and clean unsubscribe.
- The application process did not change during business tests. Existing accounts/passwords/permissions and task revisions were preserved. Test tasks/accounts were removed; test Clients were disabled with audit history retained.
- The remote support service was not restarted, and device boot IDs did not change. Installation used SSH plus normal dpkg, not OTA or a full firmware flash.

Password tests committed updates for a dedicated account in real SQLite, invoking production revocation logic. **The administrator password was not changed, and this is not a web password-change UI acceptance test.** An HTTP 200 login page is not proof of interactive login behavior.

## Fault handling repaired during validation

TaskManager/JDK companion libraries and final-package dynamic-link checks were corrected, as was the node metrics linkage declaration. The WSS example handles errors without request IDs and retries temporary 42901/50301 only for safe reads/polling/ACK within finite limits. Temporary authentication unavailability no longer incorrectly removes subscriptions as though the transport had disconnected; actual revocation still denies access.

These targeted fixes do not replace long-running concurrency tests, customer business transactions, or durable event delivery design.

## Remaining conditions

!!! warning "Legacy certificates and customer direct connections"
    The test devices' old CAs lack Key Usage, and their server SANs omit the actual LAN IPs. Tests accessed real device HTTPS/WSS through trusted SSH forwarding to match localhost SANs, with certificate-chain and hostname verification always enabled; `-k` was not used. This does not certify direct bare-LAN-IP connections with old certificates. Production needs compliant CAs, certificates covering the actual hostname/IP, and reliable time. Automatic certificate migration has not resolved the old-CA incompatibility with Python 3.13+ strict defaults.

One new SSH check after AX installation reached a 60-second timeout. The installer recorded successful dpkg completion, and subsequent independent checks, full API regression, and runtime integrity verification passed. The timeout's cause remains unconfirmed; retain it as a startup-responsiveness risk rather than asserting a proven CPU/eMMC cause or “no installation anomalies.”

This round did not exercise interactive web login/password changes, web upload/cancellation, OTA self-installation, firmware, reboot/power loss/rollback. It did not inspect video/OSD/alarm images frame by frame or certify every model, optional API, customer camera, prolonged offline period, or maximum multi-task load.

## Delivery identity

The [example ZIP 1.4.9](../../assets/downloads/aibox-openapi-examples-1.4.9.zip) is **123357 bytes**, SHA-256:

```text
306c40afa4a87b9d6f2d0a8559bc2529ee0eab5ce6d3c29058a232b9b16f0117
```

`VALIDATION.md` inside the ZIP records isolated tests; this page adds the device-specific results. Verify each ZIP file using its `MANIFEST.sha256`. The downloadable copy is the tested delivery artifact and does not include private diagnostic directories.

Follow the [examples](openapi-examples.md), then the [production checklist](openapi-examples.md#acceptance). Refer to the [full protocol](openapi-protocol.md) for field contracts.
