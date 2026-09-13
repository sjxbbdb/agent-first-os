# G5 UEFI map-key retry evidence

命令：

```text
wsl.exe bash -lc "cd '/mnt/d/Agent OS' && bash product/tests/g5-uefi-map-key-test.sh"
```

测试构建仅启用 `AGENT_OS_TEST_FORCE_MAP_KEY_FAILURE`，让第一次
`ExitBootServices` 故意使用过期 key；loader 随后重新调用 `GetMemoryMap`，
使用新 key 重试，并在 OVMF 中进入同一个 `kernel_entry`。输出：

```text
PASS: injected stale UEFI map key is recovered by GetMemoryMap retry before kernel_entry
```

这证明的是 loader 的故障恢复路径，不代表真实固件或所有硬件实现均已验证。
