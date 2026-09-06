# OpenAPI 客户调用示例

文档/示例版本 **1.4.9**，接口版本仍是 **OpenAPI v1**。本页用于客户服务端或 BFF 对接，不是盒子内部插件开发指南。

接入流程：**设备网页登录账号密码 → 领取 client_id/client_secret → 原 auth/token → 原业务接口**。无需供应商代建 Client，也无需先打开网页或等待连接图标变绿；已经持有 Client 凭据时可跳过领取。

## 1. 下载与准备

- [下载 Python、Node.js、curl 示例包 1.4.9](../../assets/downloads/aibox-openapi-examples-1.4.9.zip)
- [SHA-256 校验文件](../../assets/downloads/aibox-openapi-examples-1.4.9.zip.sha256)
- [完整中英文协议](openapi-protocol.md)、[验证记录与边界](openapi-validation.md)

ZIP 包含完整步骤 README、三种接入入口、任务/事件/录像示例、65 个 URI 的参数模板和交付检查单。源码不含实际账号、密钥、证书私钥或客户数据。示例无需安装 pip/npm 包；Python 3.10+，Node.js 22.22+。curl 入口还需要 Bash、Python 3 处理 JSON。

在下载目录验证、解压：

```bash
sha256sum -c aibox-openapi-examples-1.4.9.zip.sha256
unzip aibox-openapi-examples-1.4.9.zip
cd aibox-openapi-examples-1.4.9
sha256sum -c MANIFEST.sha256
cd TaskManager/examples/openapi
```

macOS 可用 `shasum -a 256 -c` 代替 `sha256sum -c`。后续命令均从这个 examples/openapi 目录运行。

准备能访问的设备地址、设备网页登录账号密码、可信 CA，以及支持 `clients/bootstrap` 的应用版本。使用 `https://box.customer.example:8099` 这样的 origin，不加 `/aibox/` 或业务路径；证书 SAN 必须覆盖实际域名/IP。做任务运行测试时，另准备**设备自身可访问**的 RTSP 源及对应插件/模型授权。

!!! warning "不要关闭 HTTPS 校验"
    历史设备证书可能缺少 CA Key Usage 或实际 IP 的 SAN，Python 3.13+ 的默认严格验证可能拒绝旧 CA。应更新为规范证书，不能用 `-k`、`verify=False` 或 `NODE_TLS_REJECT_UNAUTHORIZED=0` 解决。内网不等于可以明文传密码。

<a id="credentials"></a>
## 2. 用网页账号领取 Client 凭据

复制 `bootstrap.example.json` 为 `bootstrap.json`，用编辑器修改：

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

用编辑器将网页原始密码保存到 `web-password.txt`，把可信 CA 放到 `device-ca.pem`。只查设备/任务时，Scope 缩减为 `device.read` 和 `task.read`。账号必须具有申请的权限，不允许申请 `*` 或 `openapi.clients.manage`。

```bash
chmod 600 bootstrap.json web-password.txt
```

**任选一种执行一次**，不要把三条全部执行成三个 Client：

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

程序实际发送 `POST /openapi/v1/command`，JSON 中的 `uri` 是 `/openapi/v1/clients/bootstrap`，`param` 带 `username/password/display_name/scopes`，不带 Token。成功响应只包含 Client 凭据和元数据，**不会直接返回业务 Access Token**。

成功后在全新的私有目录 `issued-client/` 保存 `config.json`、`client-secret.txt`、`request-id.txt`，不覆盖已有目录、不打印 Secret。后续示例直接使用这份配置；正常业务无需保存网页密码。客户浏览器不能持有 `client_secret`，应由客户服务端/BFF 保存。

### 改密、有效期与失败处理

- 改密后，该账号领取的旧 Secret、Token 和旧 WSS 身份失效；密码改回原值也不复活。权限变更、删除/重建账号同样失效。用新密码重新领取，再重建 Token/Ticket/WSS，并按已保存游标补偿事件。
- 已被设备接受的任务不会因改密自动取消。独立管理员创建的 Client 不受无关账号改密影响。
- `client_id` 是标识，不是短期 Token；本版没有对 Client Secret 新增固定的天数到期策略。Access Token 默认 **3600 秒**，客户端缓存并提前续期，不为每个请求重新领取 Client 或 Token。
- 同账号同 `request_id` 重复领取返回 `40901 / CLIENT_SECRET_ALREADY_ISSUED`，不会再次返回 Secret。响应丢失时凭请求 ID 核查，禁止自动换新 ID 无限创建。
- 错误密码 `40101`、越权 `40301`、限流 `42901`、临时不可用 `50301` 都必须明确处理。`clients/get/list` 不能找回原 Secret；遗失后由管理员轮换或撤销，再领取。

## 3. 第一次连接：只读验证

任选一种；HTTPS 不需要建立 WebRTC 连接：

```bash
python3 python/01_connect.py --config issued-client/config.json
node node/01-connect.mjs --config issued-client/config.json
bash curl/01_connect.sh issued-client/config.json
```

程序按原 `auth/token` 获取短期 Token，并在 `X-Access-Token` 请求头中调用会话、设备、能力、节点目录和任务查询。请先完成这一步再写入任务。业务 URI 只能放在 JSON 的 `uri` 字段，实际 HTTP Path 始终是 `/openapi/v1/command`。

## 4. 创建、修改、启动、停止和删除任务

先提供测试 RTSP 地址；含密码时由安全服务配置注入，不写入 shell 历史：

```bash
export AIBOX_RTSP_URL='rtsp://camera.example/live'
```

只创建、读取和修改**新建的**示例任务：

```bash
python3 python/02_tasks.py --config issued-client/config.json --create
# 或选择 Node
node node/02-tasks.mjs --config issued-client/config.json --create
```

另一次需要完整启停与删除闭环时，显式运行：

```bash
python3 python/02_tasks.py --config issued-client/config.json --create --run --delete
# 或选择 Node
node node/02-tasks.mjs --config issued-client/config.json --create --run --delete
```

每次只操作自己新建的 `api-demo-<随机值>`。程序查询设备真实 `nodes/catalog` 和 `nodes/schema`，构造 `netclient → netserver`（拉流字段为 `rtsp_url`），校验图后创建；读取完整任务、保留节点/连线修改名称，带 `expected_revision` 保存。启停需要等待 Operation 终态并检查 runtime；删除前确认已停止。无 `--create` 时只显示帮助，不写设备。

`--create` 单独运行会保留该次新任务；后续一次 `--create --run --delete` 只清理后续那次新任务，不自动删除先前任务。完整图修改、加入算法/OSD/录像和持久化幂等请求的代码见 ZIP 中 README 与脚本。不要用空 `nodes/edges` 表示“未修改节点”，也不要无视 revision 冲突覆盖客户任务。

## 5. WSS 与可靠事件处理

Node 内置 WebSocket 的额外 CA 必须在进程启动前设置为与配置相同的可信 CA：

```bash
export NODE_EXTRA_CA_CERTS="$(pwd)/device-ca.pem"
node node/03-events-wss.mjs --config issued-client/config.json --seconds 60
```

另一个终端可运行新任务示例产生任务事件。顺序为 Token → Ticket → WSS → 等 ready → 订阅 → 快照/补偿 → poll → 演示处理/ACK → 心跳 → 取消订阅。无请求 ID 的错误帧也会处理；临时 42901/50301 只对安全读、轮询、ACK 有限退避，最多额外 3 次且共享总超时，不能盲目重试写请求。

WSS 是 **60 秒连接演示，不是持久化业务消费者**。可靠落库参考 Python：

```bash
python3 python/03_events_poll.py --config issued-client/config.json --inbox out/customer-events.sqlite --seconds 60
```

用同一 inbox 重跑可继续恢复。先在 SQLite 事务中按 `event_id` 去重、落库并推进 cursor，再 ACK。生产仍需实现业务事务/Outbox、投递进度及数据保留；40904 代表游标窗口过期，必须重建快照并对账，不能宣称补回全部历史事件。

## 6. 其他 API 与录像示例

`recipes.json` 包含协议 65 个 URI 的模板及风险提示，其中 5 个管理员 Client 管理接口不允许客户 SDK 调用。例如：

```bash
python3 prepare_request.py tasks/get task-get.json
# 在编辑器中将 REPLACE_TASK_ID 改为自己的真实任务 ID
python3 python/call.py task-get.json --config issued-client/config.json
# 或 node node/call.mjs task-get.json --config issued-client/config.json
```

写请求需要持久化唯一幂等键，并显式加 `--allow-write`；异步操作用 `--wait` 等终态。重试同一次写入须复用原请求文件。

录像示例使用真实 record_id 申请同源 Ticket，只下载前 1 MiB 以验证 Range，不覆盖已有文件，也不把业务 Token 发给下载请求：

```bash
python3 python/04_record_download.py rec_实际值 sample.part --config issued-client/config.json
# 或 node node/04-record-download.mjs rec_实际值 sample-node.part --config issued-client/config.json
```

`.part` 不保证能播放；完整下载另做长度与续传校验。本版没有公开报警原图 Ticket 接口，不得拼接内部网页或文件路径替代。

<a id="acceptance"></a>
## 7. 上线检查

逐项核对应用版本、CA/SAN/设备时间、账号权限、领取与改密失效、只读连接、新任务闭环、幂等/revision 冲突、事件落库/恢复、真实视频画面，以及客户实际负载。ZIP 的 `DELIVERY-CHECKLIST.md` 提供可填写清单，`README.md` 提供完整代码说明。

本次已完成的隔离与实机测试见[验证记录](openapi-validation.md)。示例通过不替代客户网络、视频效果、全部可选接口或长时间运行验收；本页不会自动升级、重启设备或修改客户已有任务。
