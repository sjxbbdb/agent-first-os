# 第四轮调研：确认令牌的签名、防重放、随机数与时间（v2，按验收审查意见修订）

**v2 修订点**：D1 Monocypher 审计对象与版本差异已写明并改正「报告 PDF 没打开」；D2 更正 consume 时钟自增条件；D3 补 agent-first-os 提交号；D4 BearSSL 链接改为无 www 并撤销多余降级。文末「核对备注」保留 v1 复核记录。

调研时间：2026-11-02（北京时间）。只做调研，不含内核代码。
标注规则同前几轮：[Cx] 对应文末来源清单；[仓库] 指本仓库 agent-first-os（本轮 git pull 后读的，HEAD 见来源清单）；[推断] 是我的判断，没有外部来源。

## 〇、现状（先读的东西）

- `research/round3-afos-baseline.md` 第 39、95、114、119、132、135 行：policy token 是**内核表里的句柄**，32 个槽，不是密码学令牌；确认来源是汇编 fixture，不是人；没有 Webhook，也没有签名；「由谁验证」还没拍板。
- 我自己核对了代码 [仓库]：
  - `product/kernel/include/policy_token.h`：`AgentOsPolicyTokenEntry` 只有 owner_process、capability、action_nonce、expires_at、generation、active 这几个字段。
  - `product/kernel/src/policy_token.c` 约第 73 行：`if (entry->expires_at != 0 && now >= entry->expires_at)` 判过期。
  - `product/kernel/src/kernel.c` 约第 1101–1104 行：consume 的 `now` 参数传的是 `++policy_token_clock`。也就是说，**现在的「时间」其实是一个只在特定条件下自增的计数器，不是真实时间**。更正（v2）：这个自增写在三目表达式里，只有 `bound_status == AGENT_OS_OK` 且 `!policy_emergency_paused` 时才会执行 `agent_os_policy_token_consume(..., ++policy_token_clock)`；能力查找失败（bound_status 不是 OK）或处于紧急暂停时，三目走另一支，不调用 consume，计数器**不**自增（kernel.c 1090 到 1112 行，我在盒子上读了原码）。所以「每次调用 consume 系统调用就自增」不成立。过期判断目前和墙上时间没有任何关系。**对测试的提醒**：过期用例不要假设每次调用都自增 tick，要么用测试版 syscall 显式推进时钟，要么在用例里先确认走到了 consume 分支。
  - mint 走 syscall（kernel.c 约第 1077 行），由内核把 token 写进表里。所以「谁有权 mint」现在完全由 syscall 权限决定，没有密码学保证。

**要补的本质**：令牌要从「内核表里的句柄」变成「内核之外签发、内核能独立验证的一段字节」。

---

## 一、HMAC-SHA256 和 Ed25519 怎么选

| 维度 | HMAC-SHA256 [C1][C2] | Ed25519 [C3] |
|---|---|---|
| 类型 | 对称：签发和验证用同一把密钥 | 非对称：私钥签，公钥验 |
| 内核需要持有什么 | **秘密密钥**（能验证就能伪造） | **只要公钥**（公开也没关系） |
| policyd | 必须持有同一把密钥 | 可以持有私钥，也可以只转发 |
| 外部确认渠道（所有者手机/Webhook 端） | 持有 = 又多一处能伪造的地方 | 可以持有私钥，由所有者设备直接签名，policyd 只是转发 [推断] |
| 内核里验证的代码量 | 很小：SHA-256 + HMAC，几百行 | 大一些：曲线运算 + SHA-512，几千行（见第二节的实际计数） |
| 验证开销 | 很低 | 比 HMAC 高，每次确认只验一次，对确认场景不构成问题 [推断，没有实测] |
| 密钥泄漏的后果 | 任何一处（内核内存、policyd、渠道）泄漏，就能**伪造任意令牌** | 公钥泄漏无所谓。私钥泄漏能伪造，但私钥只在一处 |
| 密钥轮换 | 新密钥要同时送到内核和签发方，送的过程本身要保密 | 内核只换公钥（可以放在启动镜像或配置里，不需要保密）；可以同时信任新旧两个公钥，用 key_id 区分 [推断] |
| 实现风险 | 比较 MAC 必须常数时间 [推断]；实现简单 | 要用经过审计的实现，自己写曲线运算风险很高；RFC 8032 专门有一节讲侧信道泄漏 [C3] |

**推荐（分场景）：**
1. **产品形态（经 Webhook 的人工确认）：用 Ed25519，内核只放公钥。** 理由：agent-first-os 的威胁模型是「agent 不可信、可能被提示注入」[仓库 DESIGN_BOOK]。用 HMAC 的话，内核里就躺着一把能伪造确认的秘密；agent 一旦拿到读内核内存的漏洞，确认机制整个失效。Ed25519 下内核里没有可偷的东西。最理想的是**私钥放在所有者那一端**（确认 App/设备），policyd 只负责转发，那么 policyd 被攻破也伪造不了 [推断]。
2. **第一个 QEMU 原型：两种都能做，我倾向直接做 Ed25519 验证。** 理由：用 Monocypher 只需要拷两个文件，不依赖 libc（见第二节），工作量和 HMAC 差得不多，省得以后再换一次格式。如果想先把防重放、过期和串口用例这条链路跑通，用 HMAC 先跑也可以，但要明确是**临时方案**，格式里留好 alg 字段。
3. **HMAC 适合的地方**：内核和同一信任域内的组件之间（例如内核给自己发的会话句柄、审计日志链式 MAC），密钥在启动时随机生成、从不出内核 [推断]。

---

## 二、小体量开源实现

行数都是我在 /tmp 把仓库拉下来后用 `wc -l` 实际数的（含注释和空行）。许可证都是从官方仓库的 LICENSE 文件或官网读到的。

| 实现 | 相关部分与行数 | 许可证（读到的原文） | 无 libc | 审计 | 备注 |
|---|---|---|---|---|---|
| **Monocypher**（commit 907e90ddd4c7，即 4.0.3 之后 12 个提交，`git describe` 为 4.0.3-12-g907e90d） | `src/monocypher.c` 3011 + `monocypher.h` 342；标准 Ed25519（SHA-512）在 `src/optional/monocypher-ed25519.c` 503 行，入口是 `crypto_ed25519_check` | `LICENCE.md`：二选一，BSD-2-Clause 或 CC-0 [C4] | 可以。官网写「no dependencies, not even on libc」[C5]；头文件只 include `stddef.h`、`stdint.h`（编译器自带的头）[C4] | Cure53 2020 年 6 月审计，**审计对象是 3.1.1**，不是本表推荐的版本：范围含 `src/monocypher.c/.h` 和 `src/optional/monocypher-ed25519.c/.h`，报告结论是发现的问题没有 High 或 Critical（Medium 3 条、Low 2 条等）[C6][C19]。版本差异见下方「审计覆盖范围」 | 注意：核心库的 `crypto_eddsa_check` 用的是 BLAKE2b，**不是标准 Ed25519**；要和外部（手机端、标准库）互通必须用 optional 里的 `crypto_ed25519_check` [C4] |
| **TweetNaCl**（20140427） | `tweetnacl.c` 809 行，含 Ed25519 签名（crypto_sign）和 SHA-512 | 官网：「self-contained public-domain C library」[C7] | 基本可以：只 include 自己的头文件；但需要外部提供 `randombytes()`（只有生成密钥/签名才用，验证不用）[C7] | 官网自称「world's first auditable high-security cryptographic library」[C7]；第三方审计**未找到** | 为了短写得很紧凑，性能低，可读性对审查者不算友好 [推断] |
| **libsodium**（Ed25519 ref10 + SHA-256/HMAC） | `crypto_sign/ed25519/ref10/open.c` 104、`crypto_core/ed25519/ref10/ed25519_ref10.c` 2896、`crypto_auth/hmacsha256/auth_hmacsha256.c` 123、`crypto_hash/sha256/cp/hash_sha256_cp.c` 415 | `LICENSE`：ISC License [C8] | 整库依赖 libc 和平台代码；单独抠文件出来要自己补依赖，工作量**未验证** | 文档站靠 JS 渲染没读到；审计**未找到** | 功能最全，体量和依赖对裸内核偏重 |
| **Mbed TLS** | 当前主仓库的密码学实现已经移到 `tf-psa-crypto` 子模块，浅克隆里是空目录，行数**未验证** | `LICENSE`：Apache-2.0 或 GPL-2.0-or-later 二选一 [C9] | **未验证** | **未找到** | 在 library/ 和 include/ 下 grep 不到 Ed25519/EdDSA 实现（只在 ssl.h 里出现），是否支持 Ed25519 **未验证** |
| **BearSSL** | `src/hash/sha2small.c` 341、`src/mac/hmac.c` 122；只有 Curve25519 的 ECDH（`ec_c25519_*.c`），**没找到 Ed25519 签名实现**（只在 `bearssl_ssl.h` 的注释里提到 ed25519 的位映射）[C10] | `LICENSE.txt`：MIT 式许可文本（「Permission is hereby granted, free of charge…」），版权 Thomas Pornin 2016 [C10] | 设计上面向嵌入式，是否完全不依赖 libc **未验证** | **未找到** | 适合只做 HMAC-SHA256 的方案 |
| **Linux 内核 lib/crypto** | `lib/crypto/sha256.c` 515 行，文件头写明提供 SHA-224/SHA-256/HMAC-SHA224/HMAC-SHA256 库函数；`lib/crypto/curve25519.c` 存在；`lib/crypto/ed25519.c` 返回 404 [C11] | 文件头 `SPDX-License-Identifier: GPL-2.0-or-later` [C11] | 依赖 Linux 内核头文件和基础设施，搬到自研内核要改造 [推断] | — | **许可证是关键**：GPL-2.0-or-later 代码拷进自研内核，整个内核的许可证都会受影响。仓库有 LICENSE_MATRIX.md，这轮没细读，要项目负责人自己定 |
| **Brad Conte crypto-algorithms** | `sha256.c` 158 + `sha256.h` 34，没有 HMAC（要自己写约 30 行包装 [推断]） | README：「released into the public domain」[C12]（仓库里没有单独的 LICENSE 文件） | 不行：include 了 `stdlib.h`、`memory.h`，要改 [C12] | README 自己写「without any quality claims」，定位是教学；审计**未找到** | 只适合 HMAC 方案的教学/原型 |

**推荐**：Ed25519 验证用 **Monocypher 的 optional ed25519**（monocypher.c + monocypher-ed25519.c，加头文件约 3900 行）。理由：许可证宽松（可选 CC-0），不依赖 libc，有公开的第三方审计，但**审计对象是 3.1.1，不覆盖推荐版本**（见下方「审计覆盖范围」）。如果只做 HMAC，SHA-512 Monocypher 也有，但 SHA-256 它没有；那就用 BearSSL 的 sha2small.c + hmac.c，或者自己按 RFC 4231 的测试向量验证一个小实现 [C2]。

---

## 三、防重放和令牌格式

### 3.1 nonce 还是单调计数

- **只用随机 nonce**：要记住所有用过且还没过期的 nonce。内存有上限，重启后表就丢了 [推断]。
- **只用单调计数**：内核只记一个「已接受的最大序号」，要求新令牌序号比它大，存储只要 8 字节。缺点是签发方必须严格递增，令牌一乱序就会误拒 [推断]。
- **建议组合使用**：每个签发方（key_id）一个单调 `seq`，加上 `expires_at` 限定有效窗口，再加一个小的「已用 seq 集合」（窗口内最多 N 个），允许小范围乱序 [推断]。和现有字段对应：`action_nonce` 保留，改由内核在动作进入「待确认」时随机生成，签名覆盖它。这样令牌只对这一次待确认动作有效，天然一次性。

### 3.2 字段建议（定长二进制，小端，方便无 libc 解析）

```
struct ConfirmTokenV1 {            // 共 160 字节（4+1+1+1+1+4+4+8×4+32+8+8+64）
  u8  magic[4];      // "AFCT"
  u8  version;       // 1
  u8  alg;           // 1 = Ed25519, 2 = HMAC-SHA256（临时）
  u8  risk;          // L1/L2/L3，对应设计书的分级
  u8  reserved;
  u32 key_id;        // 用哪把公钥验，支持轮换
  u32 boot_generation; // 内核每次启动 +1（持久化），旧启动的令牌全部作废
  u64 seq;           // 每个 key_id 单调递增
  u64 task_id;       // 任务 ID（含进程 generation，复用 process.c 的 id 编码）
  u64 capability;    // 绑定的 capability 句柄
  u64 action_nonce;  // 内核在进入待确认时随机生成，下发给 policyd
  u8  action_hash[32]; // SHA-256(规范化后的动作描述：目标、参数、范围)
  u64 issued_at;     // 签发方时间（仅供审计，内核不信任）
  u64 expires_at;    // 见 3.4
  u8  sig[64];       // 对前面所有字节签名（HMAC 时只用前 32 字节）
};
```

要点：
- `action_hash` 把令牌和**具体动作内容**绑定。agent 拿到「删除 /data/old」的批准，不能拿去删 /data。这也补上了现状报告第 95 行说的「缺范围字段」。
- `boot_generation` 和 `task_id` 防止跨启动、跨任务复用。
- 签名覆盖除 sig 之外的全部字段，并且前面带 magic 和 version，防止把别的消息当成令牌（域分离）[推断]。

### 3.3 已用记录怎么存，重启后怎么办

- 运行时：内核固定大小表（沿用现在 32 槽的结构），记录 (key_id, seq) 和过期时间，过期就回收 [推断]。
- **重启**：最简单可靠的办法是**不持久化已用 nonce**，改为持久化 `boot_generation` 和每个 key_id 的 `max_seq`，写到 virtio-block 上的固定扇区（仓库已有 VIRTIO_BLOCK_WRITE/FLUSH syscall）[仓库] + [推断]。启动时 boot_generation +1 并且先刷盘，再开始接受令牌。这样上一轮启动签的令牌全部失效。代价是重启前已批准、但还没执行的动作需要重新确认，对外部副作用来说这是更安全的方向。
- 如果持久化失败（盘坏了、写不进去）：拒绝所有令牌，系统进入「只读/待维护」状态，不要降级成不防重放 [推断]。

### 3.4 时间不可信怎么办

- 现状是 `++policy_token_clock`，只在能力查找成功且未紧急暂停时自增，过期等于「成功进入 consume 的次数」，不是时间 [仓库]。
- QEMU 里 RTC 的起点可以通过 `-rtc base=` 由宿主任意指定 [C13]；裸机上 RTC 可以被改、电池会掉电 [推断]。所以**墙上时间不能作为安全依据**。
- 建议：**过期按内核自己的单调时钟算，不按墙上时间**。
  1. 动作进入「待确认」时，内核记下单调时间 t0（TSC 或定时器 tick），并把 action_nonce 发出去；
  2. 令牌里的 `expires_at` 解释为「相对 t0 的有效时长上限」，或者由内核自己规定一个硬上限（比如 10 分钟，取两者里更小的那个）；
  3. 内核在 consume 时用 `now_mono - t0` 判过期。
  这样外部时间错了也只影响 `issued_at` 这一个审计字段。签发方和内核时钟不同步也没关系，因为起点 t0 是内核自己的 [推断]。
- 这个设计依赖 action_nonce 由内核生成，正好和 3.1 呼应：令牌只对内核记得的那次待确认有效，重启后单调时钟归零也没关系，因为 boot_generation 已经把旧令牌作废了。

---


### 审计覆盖范围与版本钉死（v2 新增）

- **审计对象**：Cure53 报告 `Audit-Report Monocypher Crypto Library 06.2020` 明写审计版本为 Monocypher 3.1.1（2020 年第 26 周，两人六天，白盒）。范围条目含 `src/optional/monocypher-ed25519.h/.c`，所以 Ed25519 的 SHA-512 版本当时在范围内。报告结论：时限内没有发现 High 或 Critical 级别的问题；发现项里有 Medium（MON-01-002 nonce 误用可能、MON-01-005 安全与不安全函数未区分、MON-01-006 Monokex 认证码验证未说明）和 Low（MON-01-001、MON-01-004）、Info（MON-01-003）[C19]。我这轮把 PDF 下载并用 pdftotext 读了，v1 里「PDF 打不开」的说法错了，已更正。
- **版本差异（我自己 git clone 后实测）**：推荐版本 907e90d 是 `4.0.3` 之后 12 个提交；从 `3.1.1` 到 907e90d 共 315 个提交；`src/monocypher.c` 相对 3.1.1 为 2307 行新增、2123 行删除，合计改动约 4430 行；`src/optional/monocypher-ed25519.c` 为 339 行新增、245 行删除。所以**审计没有覆盖现版本的代码**，不能写成「推荐版本已被审计」。
- **要用的话怎么钉**：二选一，由项目负责人定。(a) 钉 3.1.1，审计覆盖完全对应，代价是没有后续的 bug 修复和接口变化（4.x 有不兼容改动，具体清单我没读，未验证）；(b) 钉 4.0.3 或 907e90d 的具体提交，把它当作「未经第三方审计的、有审计前身的版本」，需要自己的向量测试（RFC 8032 的测试向量 [C3]）加验收审查复核。无论哪种，都在仓库里记录提交号和文件 sha256，不要用「最新」。

## 四、内核里随机数和时间的来源

| 来源 | 官方/权威文档读到的 | QEMU/OVMF 下 | 状态 |
|---|---|---|---|
| RDRAND | 指令把硬件随机数放进寄存器，CF=1 表示有值，CF=0 表示当时没有可用值（要重试）[C14] | QEMU 下要 CPU 型号暴露这个特性，用 TCG 还是 KVM 是否可用**未验证** | 读的是 felixcloutier.com 对 Intel SDM 的整理页，不是 Intel 官网（Intel DRNG 指南页抓不到内容），算二手来源 |
| RDSEED | 返回符合 NIST SP800-90B & C 的随机值，同样用 CF 标识 [C15] | 同上，**未验证** | 同上 |
| virtio-rng | QEMU 有 `rng-builtin`、`rng-random`（读宿主 /dev/random）、`rng-egd` 三种后端 [C13] | OVMF 里有 `VirtioRngDxe` 驱动，「为 virtio-rng 设备提供 EFI_RNG_PROTOCOL 实例」[C16] | 可用，但要在 QEMU 命令行加设备 |
| EFI_RNG_PROTOCOL | UEFI 2.10 规范第 37.5 节定义，有 GetInfo 和 GetRNG [C17] | 由 VirtioRngDxe 或 SecurityPkg 的 RngDxe 提供（两个源文件都存在 [C16]）。它是**启动服务里的协议**，ExitBootServices 之后能不能用，规范原文我没读到具体句子，**未验证**；按协议的一般规则推断，退出后就不能用了 [推断] | 建议：在 UEFI 阶段（stage0 加载器）用 GetRNG 取 32–64 字节种子交给内核，内核再自己扩展 [推断] |
| UEFI GetTime() | 运行时服务规范 8.3.1 节有 GetTime()；规范要求平台不支持某个运行时服务时，用 EFI_RT_PROPERTIES_TABLE 声明哪些可用 [C18] | OVMF 下的具体行为**未验证** | 只能当墙上时间参考，不能当安全依据 |
| RTC（CMOS） | QEMU `-rtc base=utc|localtime|datetime`，可以指定起始时间；`clock=host|rt|vm` 选时钟源 [C13] | 可用，可以控制 | **测过期很有用**（见第五节） |
| TSC | 本轮没找到并读过官方文档 | 仓库现在用 PIT（`AGENT_OS_TEST_TIMER_PREEMPT` 下才编译）[afos-status 第 65 行] | **未验证** |
| HPET | 本轮没找到并读过官方文档 | **未验证** | **未验证** |
| QEMU `-icount` | 打开虚拟指令计数，虚拟 CPU 按 2^N 纳秒一条指令执行，支持 record/replay [C13] | 可用 | 让时间推进**可复现**，适合测过期 |

**建议的随机数链路**：UEFI 阶段用 EFI_RNG_PROTOCOL（virtio-rng 提供）取种子，进内核后混入 RDRAND/RDSEED（有就用、CF=0 重试有上限），用一个基于哈希的 DRBG 扩展（可以用 Monocypher 的 BLAKE2b 或 Ed25519 自带的 SHA-512）[推断]。两个源都拿不到时：**拒绝签发 action_nonce**，所有外部动作卡在待确认，不要退化成用 TSC 当随机数 [推断]。

---

## 五、QEMU 验证建议

### 5.1 用例（4 条）

| 编号 | 场景 | 构造方式 | 期望 |
|---|---|---|---|
| T0 | 正常路径 | 宿主测试脚本用测试私钥对正确字段签名 | 放行，动作执行一次 |
| T1 | 签名错误 | 同 T0，翻转 sig 的 1 个 bit；另一个子用例是改 action_hash 1 个字节但不重签 | 拒绝，原因 BAD_SIG |
| T2 | 令牌过期 | 正确签名，但内核单调时钟超过有效期之后才提交 | 拒绝，原因 EXPIRED |
| T3 | 重放 | T0 成功后，把同一个令牌再提交一次；另一个子用例是重启后提交上一轮启动的令牌 | 拒绝，原因 REPLAY / STALE_BOOT |

### 5.2 串口输出格式（可被脚本 grep）

沿用仓库现有的 marker 风格（如 `SYSCALL policy token mint OK`）[afos-status 第 114 行]，每条一行、固定前缀、键值对：

```
CONFIRM verify case=T0 key=1 seq=7 task=0x100000002 result=ACCEPT reason=OK
CONFIRM verify case=T1 key=1 seq=8 task=0x100000002 result=DENY reason=BAD_SIG
CONFIRM verify case=T2 key=1 seq=9 task=0x100000002 result=DENY reason=EXPIRED mono_now=... deadline=...
CONFIRM verify case=T3 key=1 seq=7 task=0x100000002 result=DENY reason=REPLAY
CONFIRM effect case=T0 count=1
CONFIRM summary pass=4 fail=0
```

检查脚本要做三件事：每个 case 恰好一行；result/reason 和期望一致；`effect` 只对 T0 出现一次（证明被拒的令牌没有产生副作用）。case 标签只用于测试构建，产品构建里不出现 [推断]。

### 5.3 让时间可控

- **首选：内核测试构建里加一个「推进单调时钟」的测试 syscall**（只在测试宏下编译，仓库已经有 `AGENT_OS_TEST_TIMER_PREEMPT` 这种先例）[afos-status 第 65 行]。T2 直接把时钟推过 deadline，完全确定，不依赖宿主速度 [推断]。
- **其次：QEMU `-icount shift=N`**，让虚拟时间和执行指令数挂钩，结果可复现 [C13]。
- **墙上时间相关的用例**（证明内核不信 RTC）：用 `-rtc base=2030-01-01T00:00:00` 和 `base=2000-01-01` 各跑一次，期望 T0–T3 的结果完全一样 [C13]。
- 签名用的测试密钥对只放在测试目录，公钥编译进测试内核；产品内核的公钥另配 [推断]。

---

## 六、需要项目负责人拍板的事

1. **Ed25519 还是 HMAC。** 倾向产品用 Ed25519，内核只放公钥。理由见第一节：内核里不留可被偷的秘密，符合「agent 不是信任根」。
2. **私钥放哪。** 倾向放在所有者确认端（手机/独立设备），policyd 只转发；第一版可以先放在 policyd，但要在文档里写明这是过渡做法。理由：私钥在 policyd 时，policyd 被攻破就能伪造确认。
3. **引入哪个库。** 倾向 Monocypher（用 CC-0 或 BSD-2）。理由：无 libc、有审计、许可证宽松。**不要**直接拷 Linux lib/crypto，因为它是 GPL-2.0-or-later，需要和 LICENSE_MATRIX 对照后由你决定。
4. **过期按什么时间算。** 倾向内核单调时钟 + 内核生成 action_nonce，墙上时间只做审计。理由见 3.4。
5. **重启后已批准但没执行的动作怎么办。** 倾向全部作废、重新确认。理由：外部副作用宁可多问一次。
6. **随机数拿不到时怎么办。** 倾向拒绝所有外部动作（fail closed）。理由：nonce 可预测就等于可以预签令牌。

## 七、卡点

- Intel 官方 DRNG 指南页抓不到正文，RDRAND/RDSEED 只能用 felixcloutier.com 的 SDM 整理页（二手）。
- libsodium 文档站靠 JS 渲染，审计信息没读到。
- Mbed TLS 的密码学代码在 tf-psa-crypto 子模块里，浅克隆是空的，行数和 Ed25519 支持都没核实。
- UEFI 规范里关于 EFI_RNG_PROTOCOL 在 ExitBootServices 之后能否使用的原文没定位到。
- GitHub API 列 Linux lib/crypto 目录时没有返回结果，改为逐个文件试 raw 链接。

## 八、未验证清单

- RDRAND/RDSEED 在 QEMU TCG/KVM 下是否可用、要什么 `-cpu` 参数。
- TSC、HPET 的权威文档。
- OVMF 下 GetTime() 的实际行为。
- EFI_RNG_PROTOCOL 在 ExitBootServices 之后不可用（目前是推断）。
- Mbed TLS 的代码规模和 Ed25519 支持。
- BearSSL 是否完全不依赖 libc。
- libsodium 抠出单独文件的依赖工作量；libsodium、TweetNaCl、BearSSL、Mbed TLS 的第三方审计。
- 4.x 相对 3.1.1 的接口不兼容改动清单。
- Ed25519 验证在自研内核里的实际耗时（没实测）。
- 所有标 [推断] 的设计。

## 来源清单

- [C19] https://cure53.de/pentest-report_monocypher.pdf ：下载并用 pdftotext 读了全文：审计对象 Monocypher 3.1.1、范围条目（含 optional/monocypher-ed25519）、发现项 MON-01-001 到 006 及严重级别、结论「没有 High 或 Critical」。
- [仓库] https://github.com/sjxbbdb/agent-first-os ：**提交号（v2 补）**：方案规格报告记录的 HEAD 为 ad0fd51618da5a020ece90871a44e568ad746e01（2026-09-16）；我 v1 读代码时没有记录提交号，v2 复核时 clone 的当前 HEAD 为 9c56cdf2feb367b3cdf582ff610b4d616f155390（2026-11-02 15:41 北京时间），`git diff --stat ad0fd51 HEAD -- product/kernel` 输出为空，即 product/kernel 在两个提交之间没有变化，且 afos-eval 副本的 kernel.c 与 HEAD 的 kernel.c 逐字相同，所以本报告引用的 kernel.c 行号对 ad0fd51 和 9c56cdf 都成立。本轮 git pull 后读了 `product/kernel/include/policy_token.h`（全文）、`product/kernel/src/policy_token.c`（mint/consume 及第 73 行过期判断）、`product/kernel/src/kernel.c` 第 1077、1101–1104 行（consume 传 `++policy_token_clock` 作为 now）。
- [afos-status] `research/round3-afos-baseline.md` ：读了全文中与 token、确认、时间相关的行（39、65、95、114、119、132、135）。
- [C1] https://www.rfc-editor.org/rfc/rfc2104 ：RFC 2104《HMAC: Keyed-Hashing for Message Authentication》，确认了标题（正文没细读）。
- [C2] https://www.rfc-editor.org/rfc/rfc4231 ：RFC 4231，HMAC-SHA-224/256/384/512 的标识符和测试向量，确认了标题。
- [C3] https://www.rfc-editor.org/rfc/rfc8032 ：RFC 8032《Edwards-Curve Digital Signature Algorithm (EdDSA)》，读到目录里的 8.1「Side-Channel Leaks」，以及正文里「more resilient to side-channel attacks」。
- [C4] https://github.com/LoupVaillant/Monocypher （commit 907e90ddd4c78f02e0d49e90384e4197f7dcdf26）：`LICENCE.md`（BSD-2-Clause 或 CC-0 二选一）；`wc -l` 统计行数；头文件的 include；`crypto_eddsa_check`（BLAKE2b 版本）和 optional 里 `crypto_ed25519_check`、`crypto_sha512`。
- [C5] https://monocypher.org/ ：首页，读到「small, auditable」「no dependencies, not even on libc」「Third Party Audit (by Cure53)」。
- [C6] https://monocypher.org/quality-assurance/audit ：读到「audited by Cure53 in June 2020」，并链接 https://cure53.de/pentest-report_monocypher.pdf （PDF 没打开）。
- [C7] https://tweetnacl.cr.yp.to/ 与 https://tweetnacl.cr.yp.to/software.html ，以及 https://tweetnacl.cr.yp.to/20140427/tweetnacl.c ：读到「a crypto library in 100 tweets」「self-contained public-domain C library」「world's first auditable high-security cryptographic library」、版本 20140427；源码 809 行，需要外部 randombytes。
- [C8] https://github.com/jedisct1/libsodium ：`LICENSE`（ISC License）；Ed25519 ref10、HMAC-SHA256、SHA-256 相关文件的行数。
- [C9] https://github.com/Mbed-TLS/mbedtls ：`LICENSE`（Apache-2.0 OR GPL-2.0-or-later）；看到 `tf-psa-crypto` 目录。
- [C10] https://bearssl.org/git/BearSSL ：`LICENSE.txt`（MIT 式许可文本，版权 Thomas Pornin 2016）；sha2small.c、hmac.c、ec_c25519_*.c 行数；ed25519 只在 bearssl_ssl.h 注释里出现。
- [C11] https://raw.githubusercontent.com/torvalds/linux/master/lib/crypto/sha256.c ：文件头 SPDX GPL-2.0-or-later，「SHA-224, SHA-256, HMAC-SHA224, and HMAC-SHA256 library functions」，共 515 行；lib/crypto/curve25519.c 返回 200，lib/crypto/ed25519.c 返回 404。
- [C12] https://github.com/B-Con/crypto-algorithms ：README（public domain，「without any quality claims」）；sha256.c/h 行数和 include。
- [C13] https://www.qemu.org/docs/master/system/invocation.html ：QEMU 调用文档，`-rtc [base=utc|localtime|datetime][,clock=host|rt|vm]`、`-icount [shift=N|auto]...`、`-object rng-builtin/rng-random/rng-egd`。
- [C14] https://www.felixcloutier.com/x86/rdrand ：RDRAND 指令说明（Intel SDM 的非官方整理），CF=1 表示有可用随机值。
- [C15] https://www.felixcloutier.com/x86/rdseed ：RDSEED 说明，「NIST SP800-90B & C compliant random value」。
- [C16] https://raw.githubusercontent.com/tianocore/edk2/master/OvmfPkg/VirtioRngDxe/VirtioRng.c ：文件头「This driver produces EFI_RNG_PROTOCOL instances for virtio-rng devices」，BSD-2-Clause-Patent；`SecurityPkg/RandomNumberGenerator/RngDxe/RngDxe.c` 确认存在（200）。
- [C17] https://uefi.org/specs/UEFI/2.10/37_Secure_Technologies.html ：UEFI 2.10 规范第 37.5 节 EFI_RNG_PROTOCOL、GetInfo、GetRNG 的目录和定义。
- [C18] https://uefi.org/specs/UEFI/2.10/08_Services_Runtime_Services.html ：UEFI 2.10 运行时服务，8.3.1 GetTime()；不支持的运行时服务用 EFI_RT_PROPERTIES_TABLE 声明。

---

## 核对备注（内核调研复核）

- 来源 [C10] 的 https://bearssl.org/git/BearSSL（无 www）是 git 仓库地址，用 `git ls-remote` 确认可访问（返回 HEAD 7bea48e5e850），用浏览器式 HTTP 抓取会返回 404 属正常，不是链接失效。v1 里把 BearSSL 结论降为「未验证」的备注作废；BearSSL 是否完全不依赖 libc 仍是未验证。
- 其余 21 个链接复核时都能打开。Monocypher 的 LICENCE.md 确认是 BSD-2 与 CC-0 二选一。仓库 kernel.c 里 `policy_token_clock` 在 49、1104、1524 行出现，与报告第一条现状描述一致（在盒子上 afos-eval 副本里核对）。
