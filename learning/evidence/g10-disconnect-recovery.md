# G10 host disconnect recovery evidence

`product/tests/g10-disconnect-recovery-test.sh` exercises the bounded
`HttpRemoteModelAdapter` reference path. The injected transport fails the first
request with a disconnect, succeeds on the second request, and returns a
validated empty `ActionPlan`. With `maxAttempts=2`, the adapter retries once,
validates the response and returns it; no model output is executed by this
test.

定向红证据：旧实现只有单次 HTTP 请求，首次 disconnect 直接抛出
`remote model request failed: socket disconnected`，测试失败。绿证据：实现
有界 `maxAttempts` 后运行：

```text
G10 disconnect recovery: PASS (bounded retry returns validated plan)
boundary: host-only HTTP adapter; no native/QEMU remote model claim
```

验证命令：

```text
bash product/tests/g10-disconnect-recovery-test.sh
bash product/tests/g7-g10-host-loop-test.sh
```

边界：这是 Node.js host-side transport reference；没有证明原生 Ring 3
Agent、QEMU/OVMF 远程模型、持久化重试队列、指数退避、请求幂等服务端或
生产网络可用性。重试上限固定为 1–3 次，超过上限直接 fail-closed 或进入
已有 fallback。
