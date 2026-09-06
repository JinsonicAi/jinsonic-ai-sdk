# OpenAPI Customer Examples

Examples/documentation **1.4.9**, wire protocol **OpenAPI v1**. These examples are for a customer's trusted backend/BFF, not for developing internal device plugins.

The flow is **device web username/password → client_id/client_secret → existing auth/token → existing business APIs**. The customer can provision credentials without supplier intervention, SSH, or opening the device web UI first. Skip provisioning if you already have a Client pair.

## 1. Download and prerequisites

- [Python, Node.js and curl examples 1.4.9](../../assets/downloads/aibox-openapi-examples-1.4.9.zip)
- [SHA-256 checksum](../../assets/downloads/aibox-openapi-examples-1.4.9.zip.sha256)
- [Full protocol](openapi-protocol.md) and [validation scope](openapi-validation.md)

The ZIP contains runnable code, bilingual protocols, a detailed Chinese README, parameter recipes for 65 URIs, and a delivery checklist. It contains no live credentials, private certificate keys, or customer data. No pip/npm packages are required: use Python 3.10+ or Node.js 22.22+. The curl entry points also require Bash and Python 3 for JSON handling.

Verify and extract:

```bash
sha256sum -c aibox-openapi-examples-1.4.9.zip.sha256
unzip aibox-openapi-examples-1.4.9.zip
cd aibox-openapi-examples-1.4.9
sha256sum -c MANIFEST.sha256
cd TaskManager/examples/openapi
```

On macOS, use `shasum -a 256 -c` instead of `sha256sum -c`. Run the remaining commands from this examples/openapi directory.

Prepare a reachable device origin such as `https://box.customer.example:8099`, its web account/password, a trusted CA, and an application version supporting `clients/bootstrap`. Do not append `/aibox/` or a business URI to the origin. The certificate SAN must cover the actual hostname/IP. Video tests additionally require an RTSP source **reachable from the device**, supported plugins/models, and their licenses.

!!! warning "Keep TLS verification enabled"
    Historical device CAs may lack Key Usage, and server certificates may not include the LAN IP in their SAN. Python 3.13+ strict verification can reject the old CA. Provision compliant certificates; do not use `-k`, `verify=False`, or `NODE_TLS_REJECT_UNAUTHORIZED=0`. A private LAN does not make plaintext password transport safe.

<a id="credentials"></a>
## 2. Obtain Client credentials with the web account

Copy `bootstrap.example.json` to `bootstrap.json` and edit it:

```json
{
  "base_url": "https://box.customer.example:8099",
  "username": "your-web-account",
  "password_file": "web-password.txt",
  "display_name": "customer-integration",
  "scopes": ["device.read", "task.read", "task.write", "task.execute", "media.read", "alarm.read"],
  "ca_file": "device-ca.pem",
  "timeout_seconds": 30
}
```

Use an editor to store the original web password in `web-password.txt`; provide the trusted CA as `device-ca.pem`. For read-only onboarding, request only `device.read` and `task.read`. The account must have the requested permissions. Do not request `*` or `openapi.clients.manage`.

```bash
chmod 600 bootstrap.json web-password.txt
```

**Choose one entry point and run it once**, rather than creating three independent Clients:

=== "Python"

    ```bash
    python3 python/00_get_credentials.py --config bootstrap.json --output-dir issued-client
    ```

=== "Node.js"

    ```bash
    node node/00-get-credentials.mjs --config bootstrap.json --output-dir issued-client
    ```

=== "curl"

    ```bash
    bash curl/00_get_credentials.sh --config bootstrap.json --output-dir issued-client
    ```

The HTTP path is `POST /openapi/v1/command`; JSON `uri` is `/openapi/v1/clients/bootstrap`, and `param` contains `username/password/display_name/scopes`. No Token is required for this request. Success returns Client credentials and metadata, **not a business access token**.

The entry point creates a new private `issued-client/` directory containing `config.json`, `client-secret.txt`, and `request-id.txt`. It does not overwrite existing output or print the Secret. Use the generated configuration below. Routine business calls no longer need the web password. Keep the Client secret on the backend/BFF, never in browser code.

### Revocation, lifetime and failure handling

- A committed password change invalidates Clients provisioned by that account, their tokens, and their WSS authorization. Changing the password back does not revive them. Permission changes and account deletion/recreation also invalidate them. Provision again with the new password, obtain a new Token/Ticket/WSS, and recover events from the persisted cursor.
- Already accepted tasks are not automatically canceled. Independent administrator-created Clients are not invalidated by unrelated accounts changing passwords.
- `client_id` is an identifier, not a short-lived token. This release does not add a fixed age-based expiry for Client secrets. Access tokens default to **3600 seconds**; cache and renew them instead of provisioning a Client or token on every call.
- Repeating the same account/request ID returns `40901 / CLIENT_SECRET_ALREADY_ISSUED`, without replaying the Secret. If the response is lost, reconcile by request ID; do not loop with fresh IDs.
- Handle wrong credentials `40101`, missing permission `40301`, rate limiting `42901`, and temporary unavailability `50301` explicitly. `clients/get/list` cannot recover a lost Secret; an administrator must rotate/revoke it as appropriate.

## 3. First connection: read-only checks

Choose a language. HTTPS does not require a WebRTC connection:

```bash
python3 python/01_connect.py --config issued-client/config.json
node node/01-connect.mjs --config issued-client/config.json
bash curl/01_connect.sh issued-client/config.json
```

The client uses the original `auth/token`, then sends `X-Access-Token` when reading the session, device, capabilities, node catalog, and tasks. Complete this step before writing tasks. Business addresses are JSON Command URIs; the HTTP path remains `/openapi/v1/command`.

## 4. Create, edit, start, stop and delete a new task

Configure a test RTSP source. Inject password-bearing URLs through secure service configuration rather than shell history:

```bash
export AIBOX_RTSP_URL='rtsp://camera.example/live'
```

Create, read and edit a **new** example task without starting video:

```bash
python3 python/02_tasks.py --config issued-client/config.json --create
# Or choose Node
node node/02-tasks.mjs --config issued-client/config.json --create
```

For a separate complete start/stop/delete demonstration, explicitly opt in:

```bash
python3 python/02_tasks.py --config issued-client/config.json --create --run --delete
# Or choose Node
node node/02-tasks.mjs --config issued-client/config.json --create --run --delete
```

Each run operates only on its own new `api-demo-<random>` task. It reads real `nodes/catalog` and `nodes/schema`, builds `netclient → netserver` using **rtsp_url**, validates the graph, creates and reads the task, copies the full graph when renaming, and saves with `expected_revision`. Start/stop waits for Operation completion and checks runtime; deletion requires confirmation that the task stopped. Without `--create`, it only prints help.

`--create` alone retains that run's new task. A later `--create --run --delete` only cleans its own later task, not the earlier one. Full graph-editing, algorithm/OSD/recording guidance, and persisted idempotent writes are in the ZIP source and README. Empty `nodes/edges` do not mean “unchanged”; they clear the graph. Never ignore revision conflicts or test by overwriting existing customer tasks.

## 5. WSS and reliable event handling

For Node's built-in WebSocket, configure the same trusted CA before starting the process:

```bash
export NODE_EXTRA_CA_CERTS="$(pwd)/device-ca.pem"
node node/03-events-wss.mjs --config issued-client/config.json --seconds 60
```

Use a second terminal to create a test task and generate task events. The sequence is Token → Ticket → WSS → ready → subscription → snapshot/replay → poll → demo processing/ACK → heartbeat → unsubscribe. Errors without a request ID are handled. Temporary 42901/50301 errors receive bounded backoff only for safe reads, polling, and ACK, with at most three extra attempts within one overall deadline. Writes are not blindly replayed.

The WSS example is a **60-second connection demonstration, not a durable business consumer**. For transactional local persistence, use Python:

```bash
python3 python/03_events_poll.py --config issued-client/config.json --inbox out/customer-events.sqlite --seconds 60
```

Rerun with the same inbox to resume. Events are deduplicated by `event_id`, stored with the cursor in a SQLite transaction, and ACKed only after commit. Production still needs business transactions/Outbox, delivery progress, and retention. Error 40904 means the replay window expired; rebuild snapshots and reconcile history rather than claiming all events were recovered.

## 6. Other APIs and recording downloads

`recipes.json` covers 65 protocol URIs, including five administrator credential-management URIs that the customer SDK refuses. For example:

```bash
python3 prepare_request.py tasks/get task-get.json
# In an editor, replace REPLACE_TASK_ID with your real task ID
python3 python/call.py task-get.json --config issued-client/config.json
# Or node node/call.mjs task-get.json --config issued-client/config.json
```

Persist a unique idempotency key before writes, explicitly use `--allow-write`, and add `--wait` for asynchronous completion. Retry the same write with the same saved request file.

Recording examples obtain a same-origin Ticket for a real record ID, download only the first 1 MiB to test Range, never overwrite an existing file, and do not attach the access token to the download:

```bash
python3 python/04_record_download.py rec_actual_id sample.part --config issued-client/config.json
# Or node node/04-record-download.mjs rec_actual_id sample-node.part --config issued-client/config.json
```

A `.part` file is not guaranteed to play. Full downloads need length and resume checks. This protocol has no public alarm-original-image Ticket API; do not substitute internal UI/file paths.

<a id="acceptance"></a>
## 7. Before production

Verify application version, CA/SAN/time, permissions, provisioning and password revocation, read-only access, new-task lifecycle, idempotency/revision conflicts, durable events/recovery, actual video output, and real customer load. The ZIP includes `DELIVERY-CHECKLIST.md` and full source examples.

See [validation results and limitations](openapi-validation.md). Passing examples does not certify customer networking, video quality, every optional API, or long-term operation. This guide does not automatically upgrade/reboot the device or modify existing customer tasks.
