# virtio 与基础服务协议骨架（G6）

G6 把 QEMU 的 virtio 设备边界固定下来。当前内核已有一个受限的 legacy
PCI transport slice：扫描 virtio-blk/net、完成空特性协商和 queue 0 初始化，
并发现 modern virtio-input；文件、网络、
输入和窗口语义由 Ring 3 服务实现，并通过 IPC、shared memory 和 capability
传递数据；服务消息的 opcode/response ABI 见
`../userland/include/service_protocol.h`。

## 传输层边界

设备初始化必须遵循 virtio 的 feature negotiation → queue setup → DRIVER_OK
顺序；失败或设备报告 FAILED 时，驱动停止提交新请求并返回
`AGENT_OS_VIRTIO_E_RESET`。所有 descriptor 地址来自内核验证过的 DMA 页，
服务不能直接写 queue 寄存器。设备请求和响应使用
[`virtio_protocol.h`](virtio_protocol.h) 中的定宽 little-endian 记录；跨服务
IPC 的 request/response 使用 `../userland/include/service_protocol.h`。长度、
队列索引和 buffer 范围在提交前再次检查。

本阶段先覆盖 QEMU fixture：virtio-block、virtio-net、virtio-input 和
virtio-serial。PCI modern、legacy、真实硬件 IOMMU、MSI-X 细节可分阶段加入，
但不能改变服务层消息的语义。

## 服务层接口

* **File service**：接受 capability 限定的 block/file 请求，支持有界 read、
  write、flush；返回 request id、状态和实际字节数。路径解析、journal 和
  snapshot 属于 G7 policy 事务，不由 virtio 驱动决定。
* **Network service**：只传递有界 Ethernet frame 或 loopback fixture；每个
  frame 绑定 buffer capability，越界、重复回收和未知 queue 必须拒绝。
* **Input service**：把 virtio-input 事件转换为时间戳、类型、代码和值，交给
  Window service；驱动不能直接向 Agent Runtime 注入文本或动作。
* **Serial/diagnostic service**：提供早期串口和故障日志通道，不承担通用
  文件或网络权限。

服务启动、依赖和 heartbeat 遵循 `supervisor-protocol.md`。服务崩溃时，
Supervisor 撤销该 generation 的 device capability，驱动取消未完成请求，
再按 manifest 的 restart 策略恢复；内核继续运行。

## 安全与失败语义

设备返回的长度、状态和 descriptor 链都是不可信输入。坏 descriptor、queue
满、设备 reset、超时和 DMA fault 必须产生可审计事件，并 fail closed；不能
把未经验证的设备数据暴露给 Agent。外部网络发送是不可自动重放的副作用，
由 G7 journal 标记，virtio 层只报告完成状态。

## 当前未实现范围

仓库当前已在 QEMU fixture 中完成一个受限的 block sector-0 read，证明
descriptor chain、queue 提交和完成状态校验；`virtio_service_runtime.js` 另外提供
文件、网络、输入和窗口服务的 host reference fixture，验证 IPC 形状、payload
边界、capability 绑定、撤销和 request replay 拒绝。它还没有 queue 中断、
DMA/IOMMU 映射、modern input queue，或原生 Ring 3 服务进程的 QEMU 端到端证据。
`learning/evidence/g6-virtio-block.md` 明确区分了这些边界。G6 只有在 QEMU 中完成
更完整的 block 读写、loopback frame、input event、reset/崩溃恢复并由
`run-all` 保存串口日志后才可封门；真实 PC、GPU、USB、NVMe、Wi-Fi 和生产级
网络栈仍属于后续路线。
