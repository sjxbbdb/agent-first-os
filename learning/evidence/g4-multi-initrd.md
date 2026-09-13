# G4 multi-service initrd evidence

命令：

```text
wsl.exe bash -lc "cd '/mnt/d/Agent OS' && bash product/tests/g4-multi-initrd-test.sh"
```

测试使用 version 2 initrd manifest，包含 `echo` 与依赖它的 `logger` 两个服务，
并生成 service table 与两个有界 ELF image。Ring 0 验证 table、manifest index、
范围和 image 边界，然后把第一个服务交给当前 Ring 3 Supervisor fixture。输出：

```text
PASS: versioned multi-service initrd table is validated and the first service is launched fail-closed
```

当前证据证明的是多服务容器格式和 fail-closed 校验；依赖调度、多个进程同时启动
以及 task-group capability 回收仍未完成。
