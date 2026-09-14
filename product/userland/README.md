# 原生用户态入口

`userland` 保存原生 Ring 3 程序及其共用协议视图。`services` 保存 Supervisor 的服务 manifest、控制面 schema 和服务实现。当前 BIOS fixture 已能装载独立用户 `user.elf`，并通过 capability 保护的 IPC/共享页 syscall 运行；任务组、Supervisor 和动态服务装载仍依赖后续内核 gate。

Supervisor 的宿主接口需要四组能力：启动/停止/冻结任务组；等待进程退出；获取单调时钟并等待 deadline；通过被授予的 endpoint 收发控制消息。未来实现必须经原生 syscall/IPC 完成这些操作，不能把 manifest 中的资源字符串当成已授予的 capability。

`include/supervisor_protocol.h` 定义本地事件头和 heartbeat 的定宽视图，用于 IPC 编解码；它不是 JSON 内存布局，也不把用户指针交给其他进程。服务 ID 在 JSON 中采用字符串，在定宽头中使用 NUL 终止的 UTF-8/ASCII 字节序列。消息跨进程前必须进行长度、版本、类型和 generation 校验。

`include/agent_runtime_protocol.h` 定义 Runtime 与 Supervisor 之间的
`START`、`HEARTBEAT`、`CHECKPOINT` 和 `ACK` 控制面 opcode 及定宽事件视图。
它同样只经由普通 IPC 传输，不授予 capability，也不直接调用设备。BIOS/QEMU
与 OVMF 的 `g9-*-runtime-service-test.sh` 会让两个真实 Ring 3 进程完成这条
bounded 生命周期；这不等同于通用服务生成、持久化 checkpoint 或远程模型。

Supervisor 当前控制面定义见 `../services/supervisor-protocol.md`。`../tests/supervisor-protocol-test.sh` 只验证协议与主机 fixture；它不证明 Supervisor 已在 Ring 3 启动。

G6 的文件、网络、输入和窗口请求 opcode 以及 48-byte response ABI 定义在
`include/service_protocol.h`。`../services/virtio_service_runtime.js` 和
`../tests/g6-service-test.sh` 提供 host-only reference：它检查有界 payload、
capability 服务绑定、撤销和 request replay，并模拟 block/file、network frame、
input event 到 window service 的消息语义。该 fixture 不等同于原生 Ring 3
服务进程或 QEMU virtio queue 完整实现。
