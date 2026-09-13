# G5/G6 UEFI 与 virtio 契约证据

验证命令：

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g5-g6-contract-test.sh'
```

测试通过 `virtio_protocol.h` 定宽结构的编译断言，并检查 UEFI/virtio 文档明确标注未实现边界。当前没有 `BOOTX64.EFI`、OVMF 启动日志或 virtio 驱动；这些不能由契约测试代替。
