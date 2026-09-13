# day-02-compile：第一个 C 程序

## 构建

在 WSL Ubuntu 中执行：

```bash
gcc hello.c -o hello
./hello
echo $?
file hello
```

## 结果

- 程序输出：`Hello Agent OS!`
- 返回码：`0`
- 产物：x86_64 ELF、PIE、动态链接的 Linux 可执行文件

这个练习使用普通 GCC 和 Linux C 库，目的是认识源代码、编译、链接和运行。它还不是 freestanding 内核程序。
