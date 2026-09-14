# 教学实验项目

这里保存我们自己写的、用于证明某个源码机制的最小实验。目录可以按源码单元或主题组织，例如：

~~~text
projects/
├── SF-00-source-baseline/
├── SF-01-xv6-boot/
├── SF-03-axiomx-x86_64/
└── SF-04-native-kernel/
~~~

当前已有的 stage-0/day-02-compile 是历史 C/ELF 前置实验，保留原路径以便复盘。它生成的是普通 Linux 用户程序，不是 freestanding 内核。

每个新项目至少包含：

- README：目标、前置、构建和运行命令；
- 源码：只实现当前单元需要的最小行为；
- 一个可控改动点；
- 指向 learning/evidence/SF-xx-* 的证据链接；
- 与 product/ 的迁移说明。

外部仓库的完整源码不放在这里；它们位于仓库外，元数据和阅读记录在 source-track/。
