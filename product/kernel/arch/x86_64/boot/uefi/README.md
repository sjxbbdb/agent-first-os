# UEFI 启动器契约（G5）

本目录保留 x86_64 UEFI 启动器的架构边界。当前已具备两个可验证纵切：
`uefi_stage0.c` 证明 PE/COFF/FAT ESP 入口，`uefi_loader.c` 进一步读取并校验
`AGENTOS/KERNEL.ELF`、构造 BootInfo v2、调用 `ExitBootServices`，并在 OVMF
中进入与 BIOS 相同的 `kernel_entry`，并在有 VGA 的 OVMF 配置中发布 GOP/ACPI
提示；可选 `AGENTOS/INITRD.BIN` 也已有 OVMF 交接证据。通用重定位和完整负向
矩阵仍未完成，因此 G5 整体尚未封门。

## 目标文件布局

产品路径使用 FAT ESP 上的标准文件名：

```text
EFI/BOOT/BOOTX64.EFI   # product/kernel/arch/x86_64/boot/uefi/uefi_loader.c
AGENTOS/KERNEL.ELF    # 与 SeaBIOS 路径相同的 ELF64 内核
AGENTOS/INITRD.BIN    # 可选，固定格式只读 initrd
```

启动器只负责读取文件、收集平台信息、建立临时页表和跳转；进程、GDT、
TSS、IDT、内存回收以及 EFI runtime/service 指针的生命周期由内核负责。

## 唯一交接点

UEFI loader 必须调用与 BIOS 路径相同的 `kernel_entry(AgentOsBootInfoV2 *)`。
交接结构定义在 [`product/kernel/include/boot_info_v2.h`](../../../../include/boot_info_v2.h)：

* `magic`、`version`、`size` 必须严格匹配，未知扩展按 `size` 截断处理；
* `loader_type` 为 `AGENT_OS_BOOT_LOADER_UEFI`；
* `memory_map_addr/count/entry_size` 指向 loader 拷贝的稳定内存描述符数组，
  `AGENT_OS_BOOT_V2_FLAG_UEFI_MEMORY_MAP` 才能置位；
* `acpi_rsdp`、framebuffer 字段是可选物理地址，缺失时保持零并清除对应标志；
* `kernel_phys_*`、`staging_phys_*` 覆盖实际占用范围，供内核 PMM 保留；
* 结构及其描述符必须位于 `ExitBootServices` 后仍可访问的内存中。

UEFI 指针、`EFI_SYSTEM_TABLE`、boot-service 函数表和 map key 不得写入
BootInfo，也不得在 `kernel_entry` 之后解引用。

## 预期阶段与不变量

1. PE/COFF 入口验证 `EFI_LOADED_IMAGE_PROTOCOL` 和文件系统协议；缺失协议
   立即返回错误，不调用 BIOS 中断。
2. 读取并边界检查 ELF64：仅接受 ELFCLASS64、x86_64、`PT_LOAD`，拒绝整数
   溢出、重叠段、越界段和不可装载的入口。
3. 从 UEFI memory map 选择页框，复制内核、initrd 和 BootInfo；临时页表
   只服务于跳转，不把 loader 私有页当作可回收内存。
4. 获取 GOP 和 ACPI RSDP；它们是能力提示，不是启动成功的前提。
5. 调用 `GetMemoryMap`，若 `ExitBootServices(ImageHandle, MapKey)` 因 key
   变化失败，重新获取 map 并有限次重试。超过上限必须 fail closed。
6. 关闭 boot services 后设置内核约定的段寄存器、栈和页表，跳入
   `kernel_entry`。内核随后接管 GDT/TSS/IDT。

## 失败矩阵

| 条件 | 行为 |
| --- | --- |
| `KERNEL.ELF` 缺失或读取失败 | 显示短错误并返回 UEFI，不跳内核 |
| ELF magic/class/machine/段边界错误 | 拒绝启动，不执行任意入口 |
| 内存图为空、重叠或无法保留装载区 | 拒绝启动 |
| `ExitBootServices` key 变化 | 重新收集 map，超过有限重试后拒绝启动 |
| GOP/ACPI 缺失 | 清除对应 flag，继续启动 |
| 版本或结构大小不兼容 | 拒绝启动并保留诊断码 |

## 验收边界

G5 只有在以下证据齐备后才算完成：clang/lld 生成可加载 PE/COFF、FAT ESP
可被 OVMF 读取、OVMF 串口 marker 与 SeaBIOS 相同、坏 ELF 和 map-key 变化
测试均拒绝或降级正确。仅有此契约、主机单元测试或能编译的 stub 不计为
UEFI 启动证据。
## 当前未实现范围

完整 loader 目前只接受链接到 1 MiB、位于低端 identity window 的内核 ELF，
通用重定位、复杂 initrd 格式和完整负向矩阵仍待补齐；坏 ELF 与强制 map-key
变化已有专项 OVMF 证据。对应的 UEFI 内核运行证据见
`learning/evidence/g5-uefi-kernel.md`，initrd 证据见
`learning/evidence/g4-uefi-initrd.md`；Stage0 证据见
`learning/evidence/g5-uefi-stage0.md`。
