# G2 用户 ELF 装载证据

`product/kernel/src/elf_user_loader.c` 现在提供 freestanding、明确限定为 System V ELF64 `ET_EXEC` 的 `PT_LOAD` 规划与内存装载：校验 ELF64/x86_64、程序头边界、文件/内存大小、用户地址上限、页对齐、段重叠、入口落在可执行文件字节、未知 program header 和 W^X；装载前完整检查目标范围，随后复制文件部分并清零完整映射页尾部。

主机测试覆盖坏 magic、入口不在可执行段、W+X、`p_filesz > p_memsz`、空段、PT_INTERP/未知段、同页冲突、目标内存越界、别名和成功装载；脚本同时运行 ASan/UBSan 版本。BIOS Ring 3 fixture 还把独立构建的 `user.elf` 嵌入内核，启动时由 Ring 0 解析并复制到独立用户映射，串口出现：

```text
G2 ELF user load OK
```

本轮补充了有界的 initrd 服务镜像选择器 `agent_os_initrd_select_service`：Ring 0 会校验固定版本容器、服务表、每个镜像范围和索引，再返回指定服务 ELF 的只读指针；当前 BIOS fixture 仍明确选择 service 0，尚未把 Supervisor 的服务依赖/策略选择接入内核。主机定向测试先在旧实现上因缺少选择器编译失败（红证据），实现后通过第二个服务镜像选择和越界索引拒绝（绿证据）。

本轮另增加 host-only `agent_os_elf64_plan_with_bias`：合法 `ET_DYN` 只能使用非零页对齐 `load_bias`，入口和每个 `PT_LOAD` 都经过溢出、用户上限、段重叠和 W^X 校验；原 `ET_EXEC` API 保持兼容。定向测试先因旧实现缺少该符号链接失败，随后通过 PIE bias、未对齐 bias 和上限拒绝。

验证命令：

```text
# 旧实现定向红证据：g2_initrd_dynamic_host.c 无法编译 initrd 选择器
wsl.exe bash -lc "cd '/mnt/d/Agent OS' && bash product/tests/g2-elf-loader-test.sh"
# 实现后主机 ELF + initrd 选择器（含 ASan/UBSan ELF 重跑）
wsl.exe bash -lc "cd '/mnt/d/Agent OS' && bash product/tests/g2-elf-loader-test.sh"
# BIOS/QEMU Ring 3 回归
wsl.exe bash -lc "cd '/mnt/d/Agent OS' && bash product/tests/g2-kernel-test.sh"
```

未验证边界：`ET_DYN` 目前仅是 host loader planning API，未接入 BIOS/QEMU 真实进程创建；这不是动态链接器或文件系统装载。仅支持受校验的 ET_EXEC/ET_DYN、有限 `PT_LOAD` 和固定 initrd 容器。当前地址空间仍是有界 identity-mapped bootstrap pool，service 0 的 ELF 映射仍限制在 `user_load_storage`；尚未证明真实 PIE Ring 3 启动、物理帧分配、`PT_INTERP`、动态重定位、共享库或真实文件服务请求。

验证命令：

```text
wsl.exe bash -lc "cd '/mnt/d/Agent OS' && bash product/tests/g2-elf-loader-test.sh"
wsl.exe bash -lc "cd '/mnt/d/Agent OS' && bash product/tests/g2-kernel-test.sh"
```
