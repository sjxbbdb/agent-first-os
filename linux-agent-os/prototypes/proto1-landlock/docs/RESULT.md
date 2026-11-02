# 结果（T1、T2）
运行环境：QEMU 10.0.13，TCG（/dev/kvm 无权限），日期 2026-11-02（UTC+8）。

## T1 验收
| # | 标准 | 结论 | 证据 |
|---|---|---|---|
| 1 | 内核构建成功，.config 含 CONFIG_SECURITY_LANDLOCK=y 并按规格启用 landlock | 通过 | logs/kernel-build.log：`BUILD_RC=0 ELAPSED_S=184`；artifacts/kernel.config：`CONFIG_SECURITY=y`、`CONFIG_SECURITY_LANDLOCK=y`、`CONFIG_LSM="landlock,lockdown,yama,loadpin,safesetid,selinux,smack,tomoyo,apparmor,ipe,bpf"`；并用命令行 `lsm=landlock,lockdown,yama,loadpin,safesetid,selinux,smack,tomoyo,apparmor,ipe,bpf` |
| 2 | initramfs 能被该内核启动并进入 shell | 通过 | logs/qemu-t1-serial.log：`[    0.531527] landlock: Up and running.`、`T1_SHELL_OK: shell 可用, pid=59`、`=== T1 自检结束, poweroff ===`、`### qemu exit code: 0` |
| 3 | 留存构建命令、完整构建日志、.config、内核版本号、校验和 | 通过 | scripts/02-build-kernel.sh、logs/kernel-build.log、artifacts/kernel.config、artifacts/defconfig.config、artifacts/defconfig.diff、artifacts/VERSION.txt、artifacts/CHECKSUMS.txt、logs/download-verify.log（`linux-6.18.60.tar.xz: OK`） |
| 4 | 可由别人原样复跑 | 部分验证 | 每个分步脚本都在本机实际跑通；`run-all.sh` 合并脚本没从空目录完整跑过；没在第二台机器上复跑 |

## T2 验收
| # | 标准 | 结论 | 证据 |
|---|---|---|---|
| 1 | 测试程序源码入库、静态编译成功 | 通过（"入库"=放在 src/，按要求未 git 提交） | src/landlock_test.c；logs/test-build.log（0 warning，`Elf file type is EXEC`，无 INTERP 段 = 静态） |
| 2 | QEMU 按规格命令启动，串口全量日志落文件，含 Up and running | 通过 | logs/qemu-t2-serial-run1.log / run2.log 第 3 行为完整命令；第 128 行 `landlock: Up and running.` |
| 3 | C0~C4 与期望逐条一致 | 通过（两次运行） | 见下表 |
| 4 | ABI 返回值记入日志 | 通过 | `LANDLOCK_ABI 返回值=7` |
| 5 | 留存 QEMU 命令行、串口日志、测试输出、改动文件清单与 GPL 说明 | 通过 | 串口日志、GPL_AND_FILES.md |
| 6 | 不符如实报告 | 无不符项 | — |

ABI 实测 7；降级：未发生（`handled_access_fs 0xffff -> 0xffff`，程序按头文件认识的全部 16 个 fs 权限，ABI≥5 不需降级）。

| 用例 | 动作 | 期望 | 实际（run1 / run2） | errno |
|---|---|---|---|---|
| C0 | restrict_self 前写 /etc/x（之后 unlink） | 成功 | 成功 / 成功 | 0 |
| C1 | 写 /tmp/work/a | 成功 | 成功 / 成功 | 0 |
| C2 | 写 /etc/x（O_CREAT，文件已被 C0 后删掉） | EACCES | 失败 / 失败 | 13 EACCES |
| C3 | 读 /root/secret | EACCES | 失败 / 失败 | 13 EACCES |
| C4 | fork+execve 自身 `c4child`，子进程写 /etc/x | EACCES | 失败 / 失败 | 13 EACCES |

两次运行从"=== T2: 运行"到"T2 结束"之间输出 diff 完全相同。

## 说明 / 局限
- C0 写成功后立即 unlink /etc/x，所以 C2 是"新建文件"被拒（MAKE_REG/WRITE_FILE 未授予）。
- C3 对照：**已补**（2026-11-02 追加 C3c，见下节）。
- 只读授权目录：/bin、/sbin、/usr（initramfs 里只有这些放可执行文件；测试程序和 busybox 都静态链接，没有 /lib）。没授予 `/`。
- 实际启用的 LSM：`capability,landlock,selinux`（lsm= 列表里其他项没编进 defconfig 内核，被内核忽略）。
- KVM 未用（权限问题），用 TCG；结果与加速方式无关但如实记录。
- PGP：tarball 签名（linux-6.18.60.tar.sign，Greg KH key 647F...693E）与 sha256sums.asc（kernel.org autosigner）均 `Good signature`，但 gpg 提示 key 未经信任链认证（公钥按指纹从 keyserver 取）。

## 追加：C3c 对照（2026-11-02，只改用户态程序与 initramfs，内核未重编）
- 改动：src/landlock_test.c 在 C0（写后即删 /etc/x）之后、restrict_self 之前加 C3c「读 /root/secret，期望成功」。C0~C4 的动作和期望没改；只把末尾汇总文案改成「C0~C4(含 C3c) 全部与期望一致」。
- 复用同一份 bzImage（sha256 822c9098d5358bdf25ccc39213e295f951d5d09d05116e0dede78ac6c0a6dffa，运行前后核对未变），run-qemu.sh 命令不变，TCG。
- 新日志：logs/qemu-t2-serial-c3ctl-run1.log、logs/qemu-t2-serial-c3ctl-run2.log；构建日志 logs/test-build-c3ctl.log（0 warning）、logs/initramfs-build-t2-c3ctl.log。旧日志未覆盖。
- 新 initramfs-t2 sha256：90701128d26742b170d05da5fe015bb0a9509f2265f16e445ac720345fec8c42；新 landlock_test sha256：f3664393bb1774385beecfe751a57ac1afa3515c7cf3019cc9b987208602420e。旧值存于 artifacts/*.pre-c3c.sha256（旧日志对应旧值；旧二进制已被覆盖，可用旧版源码重建）。

关键行（run1；run2 除时间戳 `[    0.518994]` 外相同）：
```
[    0.498392] landlock: Up and running.
LANDLOCK_ABI 返回值=7
降级: 未发生 (handled_access_fs 0xffff -> 0xffff)
[C0] 动作=受限前 写 /etc/x 期望=成功 实际=成功 errno=0(-) 一致=是
[C3c] 动作=受限前 读 /root/secret 期望=成功 实际=成功 errno=0(-) 一致=是
[C1] 动作=写 /tmp/work/a 期望=成功 实际=成功 errno=0(-) 一致=是
[C2] 动作=写 /etc/x 期望=Permission denied 实际=失败 errno=13(Permission denied) 一致=是
[C3] 动作=读 /root/secret 期望=Permission denied 实际=失败 errno=13(Permission denied) 一致=是
[C4] 动作=子进程(execve) 写 /etc/x 期望=Permission denied 实际=失败 errno=13(Permission denied) 一致=是
汇总: C0~C4(含 C3c) 全部与期望一致 (不一致条数=0)
```
两次运行测试输出段 diff 完全一致；两次 qemu exit code 0。结论：同一进程受限前能读 /root/secret、受限后 EACCES，C3 的拒绝来自 Landlock。

## 送审基准（2026-11-02 冻结前追加）
旧日志 logs/qemu-t2-serial-run1.log、logs/qemu-t2-serial-run2.log、logs/qemu-t1-serial.log 对应的旧产物（加 C3c 之前的旧版 landlock_test 和旧 initramfs-t1/t2）已被覆盖、不再存在，这几份日志只算历史记录。**送审以 logs/qemu-t2-serial-c3ctl-run1.log、logs/qemu-t2-serial-c3ctl-run2.log 和冻结目录 frozen-v1/ 为准。**（注：T1 的 initramfs-t1 当前文件仍在 build/out，但未随 frozen-v1 冻结。）
