# MIT 许可证落地方案（草案）
版本：v0.2　日期：2026-11-02　状态：总经理转达项目负责人已批准，已按下面第六节执行
依据：`decisions/0005-license-mit.md`（只决定「新增文件占位许可证选 MIT」）。本文不是法律意见，只是工程落地的做法。

## 一、要定的事（我给了倾向，没有替总经理定）
| 问题 | 倾向 | 理由 |
|---|---|---|
| 版权声明署名 | `Copyright (c) 2026 agent-first-os contributors` | 用仓库名加通用写法，不含任何个人信息；具体名字总经理定 |
| 仓库根 `LICENSE` 现在放不放 | 放，内容是标准 MIT 文本，加上面的版权行 | 有了 LICENSE 文件才能让 SPDX 标识有对应的全文；但这是动仓库根，等总经理批准 |
| 已有文件是否改成 MIT | 本轮不改，只管新增文件 | 决定 0005 只说新增文件；改已有文件会让很多文件的提交记录变动，且 `product/docs/LICENSE_MATRIX.md` 写的是「待发布决策」，要同步改，另行决定 |
| `product/docs/LICENSE_MATRIX.md` 的「许可证待发布决策」 | 总经理批准 LICENSE 后同步改成「新增文件 MIT，已有文件按单独决定」 | 避免两处说法不一致 |

## 二、SPDX 标识怎么写（只对新增的自研文件）
- C、头文件：首行 `// SPDX-License-Identifier: MIT`
- 汇编（NASM）：首行 `; SPDX-License-Identifier: MIT`
- shell、Python：shebang 之后第二行 `# SPDX-License-Identifier: MIT`
- JavaScript：首行 `// SPDX-License-Identifier: MIT`
- Markdown 文档、日志、数据文件：不加。
- 仓库现状：只有 `linux-agent-os/prototypes/proto1-landlock/src/landlock_test.c` 带标识，是 `GPL-2.0-only`，不改。

## 三、不适用 MIT 的例外（需要写明，不要被 LICENSE 一刀切盖住）
1. 凡是基于或复制自 Linux 内核的补丁、模块，必须保持 GPL-2.0-only（内核的许可证要求）；M1 走自研内核（决定 0003），自研代码里不得复制 Linux 源码，复制了就不能标 MIT。
2. `proto1-landlock` 已标 GPL-2.0-only，保持。
3. 第三方代码保留各自的许可证，来源、版本或提交、许可证文本记入清单（如 Monocypher 的 BSD-2-Clause 或 CC-0，EDK2、gnu-efi 一类），随文件放原始许可证头，不改成 MIT。
4. 依赖 MIT 的库（如 `LICENSE_MATRIX.md` 里的 pi 适配器）按其要求保留版权与许可声明（NOTICE）。

## 四、实施步骤（批准后，由我做，不碰代码逻辑）
1. 在仓库根新增 `LICENSE`（标准 MIT 全文加版权行）。
2. 改 `product/docs/LICENSE_MATRIX.md` 一行，记录决定 0005。
3. 规格 `specs/native-kernel-m1.md` 通用门槛 6 写明新增自研文件的 SPDX 为 `MIT`，例外按第三节。
4. 新增的代码文件由 codex工具人在写文件时加 SPDX 头，不单独批量改已有文件。
5. 每次提交后自扫：`rg -L 'SPDX-License-Identifier' <新增代码文件>` 应为空；版权行不含个人姓名。

## 五、验证
- 对每个新增代码文件跑 `rg -n 'SPDX-License-Identifier' <文件>`，标识为 `MIT`，例外文件标识与第三节一致。
- 核对 `LICENSE` 全文与 SPDX 官方的 MIT 文本一致（逐字比对，不凭记忆写）。
- 扫描版权行与全仓不含个人姓名。

## 六、批准结果与执行记录（v0.2）
批准（经总经理转达，2026-11-02）：仓库根放标准 MIT `LICENSE`，整个仓库按 MIT，署名 `Copyright (c) 2026 agent-first-os contributors`；proto1-landlock 与 Linux 衍生补丁保持 GPL-2.0-only；已有 C 源码文件头先不动，清单交总经理。对应第一节：a 署名按倾向；b 放；c 已有文件的许可证按「整个仓库按 MIT」处理，但文件头不改；d 保持 GPL-2.0-only。
执行：
1. 根目录新增 `LICENSE`：SPDX 官方 MIT 文本，只替换版权行，逐字比对一致。
2. `product/docs/LICENSE_MATRIX.md` 的项目自身许可证一行与开头说明已改。
3. 决定 0005 补记了第二次决定。
4. 占位 `LicenseRef-AgentFirstOS-TBD` 在仓库里没有出现，没有可改的；以后新文件头直接写 `MIT`。
5. 已有源码文件头清单（只读统计，196 个文件，只有 1 个有 SPDX，且是 GPL-2.0-only 的那个）交总经理，不动文件。
