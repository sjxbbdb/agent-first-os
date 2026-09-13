# G9 HTTP remote adapter evidence

日期：2026-09-13

先红：实现尚未导出 `HttpRemoteModelAdapter` 时，定向测试失败：

```text
TypeError: HttpRemoteModelAdapter is not a constructor
```

后绿：

```text
G9 HTTP remote adapter: PASS
boundary: real HTTP-shaped Ring 3 transport with injected fetch; no provider, credential, or production model claim
```

验证了 JSON `POST`、请求 envelope、loopback Node HTTP fetch、HTTP 错误回退 `OfflineNullAdapter` 和返回 ActionPlan 的任务绑定校验。测试注入 fetch 函数，不代表已连接 pi、Codex、任意云厂商或生产远程模型。
