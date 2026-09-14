# 内核调用链地图

调用链是每个源码单元的边界。读链条时只打开当前箭头两端的文件，先验证入口，再扩展一个函数。

## SF-01：xv6 从 BIOS 到 Shell

~~~text
bootasm.S
  → bootmain.c
  → 读取 ELF program headers 并装入 kernel segments
  → entry.S
  → main.c:main()
  → userinit()
  → scheduler()
  → initcode.S
  → exec("/init")
  → init.c
  → sh.c
~~~

Makefile、bootblock 和 kernel.ld 决定镜像与链接布局，是这条运行链的构建输入，不是 CPU 依次调用的运行时函数。

需要回答：

- BIOS 把什么放在 0x7c00？
- bootmain.c 怎样把内核从磁盘读入内存？
- entry.S 为什么要先准备栈再调用 C？
- userinit() 创建的第一个用户程序怎样变成 Shell？

## SF-02：xv6 的四条机制链

### 地址空间

~~~text
kvmalloc()
  → setupkvm()
  → walkpgdir()
  → mappages()
  → switchuvm()
  → lcr3()
~~~

### 时钟与调度

~~~text
tvinit()
  → idt / trapasm.S
  → trap()
  → yield()
  → sched()
  → swtch.S
  → scheduler()
~~~

### 用户态系统调用

~~~text
usys.S
  → int $T_SYSCALL
  → trapasm.S
  → trap()
  → syscall()
  → sys_*()
  → copyin()/copyout()
  → iret
~~~

### 文件与 Shell

~~~text
sh.c
  → fork()/exec()/open()/read()/write()
  → syscall.c / sysproc.c / sysfile.c
  → file.c
  → fs.c / log.c / bio.c
  → ide.c
~~~

## SF-03：AxiomX x86_64 启动链

~~~text
boot/stage0/boot.asm
  → boot/stage1/pm_entry.asm
  → boot/stage2/pm_setup.asm
  → boot/stage3/longmode.asm
  → kernel/entry.asm
  → kernel/kernel.c
  → kernel/E820.c
  → kernel/pmm.c
  → kernel/vmm.c
  → kernel/idt.c + kernel/isr.c
  → kernel/pit.c
  → kernel/sched.c
  → kernel/shell.c
~~~

这条链目前停在内核 Shell。它是观察 x86_64 硬件状态的样本，不要把内核 Shell 误认为 Ring 3 用户空间。

## SF-04 之后：我们的目标链

~~~text
BIOS Stage 1/2 或 UEFI loader
  → versioned BootInfo
  → kernel_entry()
  → address space + IDT/TSS
  → scheduler
  → Ring 3 init
  → syscall / IPC / capability
  → Supervisor
  → file / network / window / input services
  → Policy Firewall
  → Agent Runtime
  → remote Model Adapter
~~~

## 每次追链的记录方式

记录四个地址：

1. 入口文件和入口符号；
2. 第一个状态转换函数；
3. 跨特权级或跨进程的边界；
4. 可观察输出或错误路径。

如果无法指出其中一个地址，就先缩小单元，不继续打开更多源码。
