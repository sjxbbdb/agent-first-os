// SPDX-License-Identifier: GPL-2.0-only
/*
 * landlock_test.c - Landlock 原型 1 测试程序（C0~C4）
 * 参考：Linux Documentation/userspace-api/landlock.rst、samples/landlock/sandboxer.c
 * 直接用 syscall()，头文件取自内核源码树 make headers 导出的 usr/include/linux/landlock.h
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/landlock.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#define SELF "/usr/bin/landlock_test"
static int fails;

/* 尝试 open，返回 0 成功，否则返回 errno */
static int try_open(const char *p, int fl)
{
	int fd = open(p, fl, 0644);
	if (fd < 0) return errno;
	if (fl & (O_WRONLY | O_RDWR)) (void)!write(fd, "x\n", 2);
	close(fd);
	return 0;
}

static void report(const char *id, const char *act, int want, int got)
{
	int ok = (want == got);
	printf("[%s] 动作=%s 期望=%s 实际=%s errno=%d(%s) 一致=%s\n", id, act,
	       want ? strerror(want) : "成功", got ? "失败" : "成功", got,
	       got ? strerror(got) : "-", ok ? "是" : "否");
	if (!ok) fails++;
	fflush(stdout);
}

static int add_path(int rs, const char *p, __u64 acc)
{
	struct landlock_path_beneath_attr pb = { .allowed_access = acc };
	pb.parent_fd = open(p, O_PATH | O_CLOEXEC);
	if (pb.parent_fd < 0) { printf("open %s: %s\n", p, strerror(errno)); return -1; }
	int r = syscall(SYS_landlock_add_rule, rs, LANDLOCK_RULE_PATH_BENEATH, &pb, 0);
	if (r) printf("add_rule %s: %s\n", p, strerror(errno));
	close(pb.parent_fd);
	return r;
}

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "c4child")) {  /* C4 子进程：重复 C2 */
		int e = try_open("/etc/x", O_WRONLY | O_CREAT | O_TRUNC);
		report("C4", "子进程(execve) 写 /etc/x", EACCES, e);
		return fails ? 1 : 0;
	}
	setvbuf(stdout, NULL, _IOLBF, 0);
	int abi = syscall(SYS_landlock_create_ruleset, NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
	printf("LANDLOCK_ABI 返回值=%d%s%s\n", abi, abi < 0 ? " errno=" : "", abi < 0 ? strerror(errno) : "");
	if (abi < 1) { printf("Landlock 不可用，终止\n"); return 2; }

	/* 本程序按头文件能识别的全部 fs 权限编写；按实测 ABI 降级（同文档示例） */
	__u64 fs_all = LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_WRITE_FILE |
		LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR |
		LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE |
		LANDLOCK_ACCESS_FS_MAKE_CHAR | LANDLOCK_ACCESS_FS_MAKE_DIR |
		LANDLOCK_ACCESS_FS_MAKE_REG | LANDLOCK_ACCESS_FS_MAKE_SOCK |
		LANDLOCK_ACCESS_FS_MAKE_FIFO | LANDLOCK_ACCESS_FS_MAKE_BLOCK |
		LANDLOCK_ACCESS_FS_MAKE_SYM | LANDLOCK_ACCESS_FS_REFER |
		LANDLOCK_ACCESS_FS_TRUNCATE | LANDLOCK_ACCESS_FS_IOCTL_DEV;
	__u64 fs = fs_all;
	switch (abi) {
	case 1: fs &= ~LANDLOCK_ACCESS_FS_REFER; /* fallthrough */
	case 2: fs &= ~LANDLOCK_ACCESS_FS_TRUNCATE; /* fallthrough */
	case 3: case 4: fs &= ~LANDLOCK_ACCESS_FS_IOCTL_DEV; /* fallthrough */
	default: break;
	}
	printf("降级: %s (handled_access_fs 0x%llx -> 0x%llx)\n", fs == fs_all ? "未发生" : "已发生",
	       (unsigned long long)fs_all, (unsigned long long)fs);
	__u64 ro = fs & (LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR);

	/* C0 对照：受限前写 /etc/x，然后删除，保证 C2 时 /etc/x 不存在（C2 需新建文件） */
	report("C0", "受限前 写 /etc/x", 0, try_open("/etc/x", O_WRONLY | O_CREAT | O_TRUNC));
	printf("C0 清理 unlink(/etc/x)=%d\n", unlink("/etc/x"));
	/* C3c 对照：受限前读 /root/secret，期望成功，证明 C3 的 EACCES 来自 Landlock 而非 DAC */
	report("C3c", "受限前 读 /root/secret", 0, try_open("/root/secret", O_RDONLY));

	struct landlock_ruleset_attr ra = { .handled_access_fs = fs };
	int rs = syscall(SYS_landlock_create_ruleset, &ra, sizeof(ra), 0);
	if (rs < 0) { perror("create_ruleset"); return 2; }
	if (add_path(rs, "/bin", ro) || add_path(rs, "/sbin", ro) || add_path(rs, "/usr", ro) ||
	    add_path(rs, "/tmp/work", fs)) return 2;
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) { perror("no_new_privs"); return 2; }
	if (syscall(SYS_landlock_restrict_self, rs, 0)) { perror("restrict_self"); return 2; }
	close(rs);
	printf("restrict_self 成功（只读: /bin /sbin /usr；读写: /tmp/work）\n");

	report("C1", "写 /tmp/work/a", 0, try_open("/tmp/work/a", O_WRONLY | O_CREAT | O_TRUNC));
	report("C2", "写 /etc/x", EACCES, try_open("/etc/x", O_WRONLY | O_CREAT | O_TRUNC));
	report("C3", "读 /root/secret", EACCES, try_open("/root/secret", O_RDONLY));

	pid_t pid = fork();
	if (pid == 0) { execl(SELF, SELF, "c4child", (char *)NULL); perror("execve"); _exit(127); }
	int st = 0; waitpid(pid, &st, 0);
	if (!WIFEXITED(st) || WEXITSTATUS(st)) { fails++; printf("C4 子进程退出状态=0x%x\n", st); }

	printf("汇总: %s (不一致条数=%d)\n", fails ? "有用例与期望不符" : "C0~C4(含 C3c) 全部与期望一致", fails);
	return fails ? 1 : 0;
}
