# G4 Supervisor 协议骨架证据

验证命令：

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/supervisor-protocol-test.sh'
```

验证器通过 manifest/event 的关键 schema、依赖与 heartbeat 字段、generation/sequence 规则，并拒绝受保护 root capability。当前是用户态协议和主机验证器；Supervisor 尚未作为 Ring 3 进程由 initrd 启动，崩溃回收与真实 capability 撤销属于后续 gate。
