# G6 Ring 3 服务协议 fixture 证据

`product/userland/include/service_protocol.h` 现在固定了文件、网络、输入和
窗口服务 opcode，以及 `AgentOsServiceResponse` 和 `AgentOsFileRange` 的布局。
`product/services/virtio_service_runtime.js` 是一个 host-only reference：它用
IPC 形状的请求检查 ABI/version/size、payload 上限、服务 capability 绑定与
generation、request id replay、撤销和文件范围；同时模拟 block/file read/write/flush、
Ethernet frame send/receive、输入事件轮询和输入到 window service 的投递。

验证：

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g6-service-test.sh'
G6 Ring3 file/network/input/window service fixture: PASS
evidence: bounded IPC-shaped requests, capability binding, revoke, replay, and malformed/range rejection
boundary: host reference only; no native Ring3 process or full virtio network/input queue claim
```

另外，`g5-g6-contract-test.sh` 用 `gcc -Wall -Wextra -Werror` 编译检查
`AgentOsServiceRequest` 56 bytes、`AgentOsServiceResponse` 48 bytes 和
`AgentOsFileRange` 16 bytes。这个 fixture 不能替代 QEMU virtio network/input
queue、DMA/IOMMU 或原生 Ring 3 服务进程证据；那些仍是 G6 后续工作。
