# G9 durable checkpoint evidence

日期：2026-09-13

先红：在 `DurableCheckpointStore` 尚未导出时，定向测试失败：

```text
TypeError: DurableCheckpointStore is not a constructor
```

后绿：

```text
G9 durable checkpoint store: PASS
boundary: host-side fsync/reopen checkpoint reference; no native crash-consistent filesystem claim
```

验证范围：同一任务多次 checkpoint、重新打开恢复最新快照、删除 tombstone 持久化、截断尾和非法 JSON fail-closed。该实现只证明 Agent Runtime 的 host-side 持久化边界，不证明原生文件系统的崩溃一致性、跨文件事务或真实硬件存储保证。
