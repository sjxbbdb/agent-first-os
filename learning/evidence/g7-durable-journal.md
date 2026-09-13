# G7 durable JSONL journal evidence

日期：2026-09-13

## 先红后绿

新增测试首次运行（实现尚未导出 `DurableJSONLJournal`）失败：

```text
TypeError: DurableJSONLJournal is not a constructor
```

实现完成后运行：

```text
node product/tests/g7-durable-journal.test.js
```

输出：

```text
G7 durable JSONL journal: PASS
boundary: host-side append/fsync and recovery metadata only; no real disk transaction claim
```

包装脚本为 `product/tests/g7-durable-journal-test.sh`；本次只运行了该 Node 定向测试，没有运行 `run-all.sh` 或 QEMU 测试。

## 已验证行为

- `DurableJSONLJournal` 以明确的 JSONL 文件格式追加记录，每条记录写完整换行并调用 `fsyncSync`；序列号由 journal 分配且必须连续。
- 新实例重新打开同一文件后恢复 `prepare`、`commit` 和 `rollback_pending` 的 action 状态。
- 缺少末尾换行的截断尾记录、非法 JSON、非法 UTF-8 和序列号不连续都会拒绝打开，保持 fail-closed。
- `getSnapshotMetadata(actionId)` 与 `getPreimageMetadata(actionId)` 提供可恢复的快照/前像元数据读取接口；接口只保存描述性元数据。

## 边界

这是 Node.js host-side 追加日志和恢复参考实现。`fsync` 提供的是该文件描述符的落盘请求，不构成真实文件系统事务、跨文件原子提交、硬件存储保证或原生 Ring 3/内核持久化证明。
