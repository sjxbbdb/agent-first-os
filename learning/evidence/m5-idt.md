# M5：C 内核骨架和最小 IDT 异常观测

## 验证命令

在 WSL2 Ubuntu 中运行：

```text
bash product/tests/stage1-test.sh
bash product/tests/stage4-test.sh
bash product/tests/stage5-test.sh
```

## 观察结果

正常启动路径包含：

```text
KERNEL ELF OK - jumping to kernel
IDT OK - 256 vectors
KERNEL C OK
```

`stage5-test.sh` 以 `KERNEL_CFLAGS_EXTRA=-DAGENT_OS_TEST_DIV0` 重建内核，执行除零指令后串口记录：

```text
EXCEPTION observed
vector=0x0000000000000000
error=0x0000000000000000
rip=0x0000000000100c83
EXCEPTION HALT
PASS: divide-by-zero reaches the initialized IDT, is logged, and halts
```

## 实现边界

- `interrupts.asm` 为 0–31 生成异常入口；有错误码的异常保留 CPU 压入的错误码，其余入口补零错误码。
- IDT 共有 256 项，其余向量使用统一 spurious 停机桩；当前没有 IRQ/PIC/APIC、`iret` 返回、抢占或调度器。
- C 处理器只记录向量、错误码和故障返回地址，然后停机，便于后续加入 panic、任务隔离和恢复策略。
- 该验证在 QEMU/SeaBIOS 单核环境完成；真实硬件、中断控制器和用户态异常路径尚未验证。
