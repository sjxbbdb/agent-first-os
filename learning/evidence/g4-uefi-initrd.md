# G4 UEFI initrd evidence

命令：

```text
wsl.exe bash -lc "cd '/mnt/d/Agent OS' && bash product/tests/g4-uefi-initrd-test.sh"
```

OVMF 从 FAT ESP 读取 `AGENTOS/INITRD.BIN`，loader 将其复制到低地址并在
BootInfo v2 设置 INITRD 标志；共享 `kernel_entry` 随后验证 manifest、装载
service ELF，并由 Ring 3 Supervisor fixture 启动服务。输出：

```text
PASS: OVMF UEFI loader hands an initrd to the shared kernel_entry and Supervisor spawns its service
```

这仍是一个固定格式、单服务的教学 initrd，不代表通用文件系统或生产级服务编排。
