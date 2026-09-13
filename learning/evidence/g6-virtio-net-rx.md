# G6 virtio-net RX queue evidence

The gated RX transaction configures a queue-0 device-write descriptor, publishes
it in the available ring, notifies QEMU, and polls the device-visible used
index. `RX OK` is emitted only after `used.idx` advances. With QEMU's user
network backend and no injected host packet, `RX WAIT` is a bounded queue-arm
result and the focused test exits 2; it does not claim RX completion.

Run:

```text
wsl.exe bash -lc 'cd "/mnt/d/Agent OS" && bash product/tests/g6-virtio-net-rx-test.sh'
```
