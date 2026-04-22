/*
 * test-proc-family
 *
 * 覆盖 fork / clone / clone3 / execve / wait4 / exit / exit_group /
 * getpid / getppid / gettid / set_tid_address 的正常 + 错误路径。
 * 参考 linux-compatible-testsuit/tests/test_fork_v2.c。
 *
 * 说明：
 *  - riscv64/aarch64/loongarch64 无独立 fork syscall；glibc/musl 通过
 *    clone(SIGCHLD, 0, ...) 实现 fork()，所以 fork() 在所有 arch 都可用。
 *  - 只挑 11 个 syscall 的可移植语义，signal-stop (WUNTRACED/WCONTINUED)
 *    和会话/进程组不在本测例范围。
 */

#include "test_framework.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>
#include <sched.h>
#include <stdint.h>

#ifndef __NR_clone3
#  if defined(__x86_64__)
#    define __NR_clone3 435
#  elif defined(__aarch64__)
#    define __NR_clone3 435
#  elif defined(__riscv)
#    define __NR_clone3 435
#  elif defined(__loongarch__)
#    define __NR_clone3 435
#  endif
#endif

/* clone3 参数（man 2 clone3） */
struct clone3_args {
    uint64_t flags;
    uint64_t pidfd;
    uint64_t child_tid;
    uint64_t parent_tid;
    uint64_t exit_signal;
    uint64_t stack;
    uint64_t stack_size;
    uint64_t tls;
    uint64_t set_tid;
    uint64_t set_tid_size;
    uint64_t cgroup;
};

#define TMPFILE "/tmp/starry_proc_test.tmp"

/* 本测例二进制的路径；在 main() 里被 argv[0] 初始化。execve 自回环用。 */
static char self_path[256] = "/usr/bin/test-proc-family";

static int global_cow_var = 42;

/* ---------- PART 0: execve 自回环模式 ---------- */
static int handle_self_check(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--exec-ok") == 0) {
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "--exec-code") == 0 && argc > 2) {
        return atoi(argv[2]);
    }
    if (argc > 2 && strcmp(argv[1], "--exec-check-pid") == 0) {
        /* 校验 post-exec 的 pid 等于 argv[2] */
        pid_t expected = (pid_t)atoi(argv[2]);
        return (getpid() == expected) ? 0 : 1;
    }
    return -1;
}

/* ---------- PART 1: PID / TID ---------- */
static void test_getpid_getppid_gettid(void)
{
    pid_t pid = getpid();
    CHECK(pid > 0, "getpid > 0");

    pid_t ppid = getppid();
    CHECK(ppid > 0, "getppid > 0");
    CHECK(ppid != pid, "getppid != getpid");

    pid_t tid = gettid();
    CHECK(tid > 0, "gettid > 0");
    /* 主线程 tid == pid */
    CHECK(tid == pid, "主线程 gettid == getpid");
}

/* set_tid_address 返回当前 tid（man 2 set_tid_address: "always returns the
 * caller's thread ID"） */
static void test_set_tid_address(void)
{
    int clear_tid = 0;
    long rc = syscall(SYS_set_tid_address, &clear_tid);
    CHECK(rc > 0, "set_tid_address 返回正值");
    CHECK(rc == (long)gettid(), "set_tid_address 返回 == gettid()");
}

/* ---------- PART 2: fork + wait4 基础 ---------- */
static void test_fork_and_wait(void)
{
    pid_t parent_pid = getpid();
    pid_t pid = fork();
    if (pid == 0) {
        /* 子进程：验证 ppid 与父进程 pid 对上 */
        if (getppid() != parent_pid) {
            _exit(77);
        }
        _exit(42);
    }
    CHECK(pid > 0, "fork 父返回子 pid");

    int status = 0;
    pid_t waited = wait4(pid, &status, 0, NULL);
    CHECK_RET(waited, pid, "wait4 返回正确 pid");
    CHECK(WIFEXITED(status), "子进程正常退出 (WIFEXITED)");
    CHECK_RET(WEXITSTATUS(status), 42, "WEXITSTATUS == 42");
}

/* fork 写时复制 */
static void test_fork_cow(void)
{
    pid_t pid = fork();
    if (pid == 0) {
        global_cow_var = 99;
        _exit(global_cow_var);
    }
    int status = 0;
    wait4(pid, &status, 0, NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 99,
          "CoW: 子进程 exit(99)");
    CHECK(global_cow_var == 42, "CoW: 父进程 global_cow_var 仍为 42");
}

/* fork 后 fd 继承 */
static void test_fork_fd_inheritance(void)
{
    unlink(TMPFILE);
    int fd = open(TMPFILE, O_CREAT | O_RDWR | O_TRUNC, 0644);
    CHECK(fd >= 0, "open TMPFILE");

    const char *msg = "hello";
    ssize_t w = write(fd, msg, 5);
    CHECK(w == 5, "父写入 5 字节");

    pid_t pid = fork();
    if (pid == 0) {
        /* 子：通过继承 fd 回读 */
        char buf[16] = {0};
        lseek(fd, 0, SEEK_SET);
        ssize_t r = read(fd, buf, 15);
        if (r == 5 && memcmp(buf, "hello", 5) == 0) {
            _exit(0);
        }
        _exit(1);
    }
    int status = 0;
    wait4(pid, &status, 0, NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "fd 继承: 子进程读到 'hello'");

    close(fd);
    unlink(TMPFILE);
}

/* ---------- PART 3: wait4 variants ---------- */

/* WNOHANG: 子进程未退出时 wait4 立刻返回 0 */
static void test_wait4_wnohang(void)
{
    pid_t pid = fork();
    if (pid == 0) {
        /* 睡一会儿再退出 */
        usleep(200 * 1000);
        _exit(0);
    }
    int status = 0;
    pid_t r = wait4(pid, &status, WNOHANG, NULL);
    CHECK_RET(r, 0, "wait4 WNOHANG 子运行时返回 0");

    /* 阻塞回收 */
    pid_t w = wait4(pid, &status, 0, NULL);
    CHECK_RET(w, pid, "wait4 阻塞: 收回子进程");
    CHECK(WIFEXITED(status), "阻塞 wait4: WIFEXITED");
}

/* wait4 -1 + 多子进程 */
static void test_wait4_any_multi_children(void)
{
    pid_t kids[3];
    int expected[3] = {5, 6, 7};
    for (int i = 0; i < 3; i++) {
        kids[i] = fork();
        if (kids[i] == 0) {
            _exit(expected[i]);
        }
    }

    int got_code[3] = {-1, -1, -1};
    int collected = 0;
    for (int i = 0; i < 3; i++) {
        int status = 0;
        pid_t w = wait4(-1, &status, 0, NULL);
        if (w > 0) {
            collected++;
            for (int j = 0; j < 3; j++) {
                if (w == kids[j] && WIFEXITED(status)) {
                    got_code[j] = WEXITSTATUS(status);
                }
            }
        }
    }
    CHECK_RET(collected, 3, "wait4 -1 回收了 3 个子进程");

    int ok = (got_code[0] == 5 && got_code[1] == 6 && got_code[2] == 7);
    CHECK(ok, "每个子进程 WEXITSTATUS 与预期匹配");
}

/* wait4 空子进程池 → ECHILD */
static void test_wait4_echild(void)
{
    int status = 0;
    /* 确保没有活着的子进程：连续 wait 到 ECHILD */
    while (wait4(-1, &status, WNOHANG, NULL) > 0) {
        /* drain */
    }
    errno = 0;
    CHECK_ERR(wait4(-1, &status, 0, NULL), ECHILD,
              "wait4(-1) 无子进程 → ECHILD");
}

/* wait4 不存在的 pid → ECHILD */
static void test_wait4_unknown_pid(void)
{
    int status = 0;
    CHECK_ERR(wait4(999999, &status, 0, NULL), ECHILD,
              "wait4(999999) → ECHILD");
}

/* ---------- PART 4: exit / exit_group ---------- */

/* exit 与 exit_group 对单线程进程效果等价，验证两个 syscall 都能把
 * 退出码传到 wait4 */
static void test_exit_code_passthrough(void)
{
    pid_t pid = fork();
    if (pid == 0) {
        /* 直接走 syscall，避免 libc 加工 */
        syscall(SYS_exit, 123);
        _exit(200);  /* 不应到达 */
    }
    int status = 0;
    wait4(pid, &status, 0, NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 123,
          "sys_exit(123) 穿透到 WEXITSTATUS");

    pid = fork();
    if (pid == 0) {
        syscall(SYS_exit_group, 77);
        _exit(200);
    }
    wait4(pid, &status, 0, NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 77,
          "sys_exit_group(77) 穿透到 WEXITSTATUS");
}

/* ---------- PART 5: clone ---------- */

/* clone CLONE_FILES: 父子共享 fd 表
 * 注意：这里仅验证 fd 表共享语义，不起新线程——child 会独立走，直接 _exit */
static void test_clone_files_share(void)
{
    int pfd[2];
    CHECK_RET(pipe(pfd), 0, "pipe()");

    pid_t pid = syscall(SYS_clone, CLONE_FILES | SIGCHLD,
                        (void *)0, (void *)0, (void *)0, (void *)0);
    if (pid == 0) {
        const char *m = "ping";
        write(pfd[1], m, 4);
        _exit(0);
    }
    CHECK(pid > 0, "clone(CLONE_FILES|SIGCHLD) 父返回子 pid");

    char buf[8] = {0};
    ssize_t r = read(pfd[0], buf, 4);
    CHECK(r == 4 && memcmp(buf, "ping", 4) == 0,
          "CLONE_FILES: 父通过共享 fd 收到子写入");

    int status = 0;
    wait4(pid, &status, 0, NULL);
    close(pfd[0]);
    close(pfd[1]);
}

/* ---------- PART 6: clone3 ---------- */

/* 运行期探测：当前内核是否支持 clone3。老 Linux host 可能 ENOSYS。 */
static int clone3_supported = -1;
static int probe_clone3(void)
{
    if (clone3_supported >= 0) {
        return clone3_supported;
    }
    /* size=0 的调用在任何支持 clone3 的内核上返回 EINVAL；不支持时 ENOSYS。 */
    errno = 0;
    long rc = syscall(__NR_clone3, (void *)0, (size_t)0);
    (void)rc;
    clone3_supported = (errno == ENOSYS) ? 0 : 1;
    return clone3_supported;
}

#define SKIP_IF_NO_CLONE3(tag) do {                                     \
    if (!probe_clone3()) {                                              \
        printf("  SKIP | %s:%d | %s (clone3 unsupported on host)\n",    \
               __FILE__, __LINE__, tag);                                 \
        return;                                                          \
    }                                                                    \
} while (0)

/* clone3 基本 happy path */
static void test_clone3_basic(void)
{
    SKIP_IF_NO_CLONE3("clone3 happy path");
    struct clone3_args ca = {0};
    ca.flags = 0;
    ca.exit_signal = SIGCHLD;

    long pid = syscall(__NR_clone3, &ca, sizeof(ca));
    if (pid == 0) {
        _exit(33);
    }
    CHECK(pid > 0, "clone3 happy path: 父返回 pid > 0");

    int status = 0;
    wait4(pid, &status, 0, NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 33,
          "clone3: WEXITSTATUS == 33");
}

/* clone3 size 过小 → EINVAL（Linux: size < sizeof(u64)*8 时返回 EINVAL） */
static void test_clone3_small_size_einval(void)
{
    SKIP_IF_NO_CLONE3("clone3 size=4 EINVAL");
    struct clone3_args ca = {0};
    ca.exit_signal = SIGCHLD;
    errno = 0;
    long rc = syscall(__NR_clone3, &ca, 4);  /* 明显太小 */
    CHECK(rc == -1 && errno == EINVAL, "clone3 size=4 → EINVAL");
}

/* clone3 DETACHED + SIGCHLD 互斥 → EINVAL */
static void test_clone3_detached_einval(void)
{
    SKIP_IF_NO_CLONE3("clone3 DETACHED EINVAL");
    struct clone3_args ca = {0};
    ca.flags = CLONE_DETACHED;
    ca.exit_signal = SIGCHLD;

    errno = 0;
    long rc = syscall(__NR_clone3, &ca, sizeof(ca));
    if (rc == 0) {
        _exit(201);
    }
    CHECK(rc == -1 && errno == EINVAL,
          "clone3 DETACHED|SIGCHLD 组合 → EINVAL");
}

/* ---------- PART 7: execve ---------- */

/* execve 成功：子 fork + exec 自己的 "--exec-ok" */
static void test_execve_self_ok(void)
{
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = {(char *)self_path, (char *)"--exec-ok", NULL};
        char *envp[] = {NULL};
        execve(self_path, argv, envp);
        _exit(255);  /* 不应到达 */
    }
    int status = 0;
    wait4(pid, &status, 0, NULL);
    CHECK(WIFEXITED(status), "execve: 子正常退出");
    CHECK_RET(WEXITSTATUS(status), 0, "execve self-check → 0");
}

/* execve 能传递参数 */
static void test_execve_self_code(void)
{
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = {(char *)self_path, (char *)"--exec-code",
                        (char *)"55", NULL};
        char *envp[] = {NULL};
        execve(self_path, argv, envp);
        _exit(255);
    }
    int status = 0;
    wait4(pid, &status, 0, NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 55,
          "execve 参数透传：exit(55)");
}

/* execve 不存在的路径 → ENOENT */
static void test_execve_enoent(void)
{
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = {(char *)"/no/such/starry/path/x", NULL};
        char *envp[] = {NULL};
        execve("/no/such/starry/path/x", argv, envp);
        /* 走到这里说明 execve 失败，把 errno 作为退出码传出去 */
        _exit(errno == ENOENT ? 0 : 1);
    }
    int status = 0;
    wait4(pid, &status, 0, NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "execve /no/such/... → ENOENT");
}

/* execve 指向一个目录 → EACCES 或 EISDIR（Linux 返回 EACCES） */
static void test_execve_directory(void)
{
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = {(char *)"/tmp", NULL};
        char *envp[] = {NULL};
        execve("/tmp", argv, envp);
        /* 父端比较 errno；通过退出码回传 */
        _exit(errno);
    }
    int status = 0;
    wait4(pid, &status, 0, NULL);
    CHECK(WIFEXITED(status), "execve 目标目录: 子正常退出");
    int err = WEXITSTATUS(status);
    CHECK(err == EACCES || err == EISDIR,
          "execve /tmp → EACCES 或 EISDIR");
}

/* ---------- PART 8: PID 关系 ---------- */

/* 父退出后，孙进程 ppid 变为 init(1) —— 在 StarryOS 下 "init" 可能是 root
 * 任务，只要求 grandchild 的 getppid() 与 original parent_pid 不一致 */
static void test_reparent_on_parent_exit(void)
{
    int p[2];
    pipe(p);

    pid_t parent_pid = getpid();
    pid_t mid = fork();
    if (mid == 0) {
        pid_t grand = fork();
        if (grand == 0) {
            /* 孙：等父（mid）退出；把 ppid 写入管道 */
            close(p[0]);
            usleep(100 * 1000);
            pid_t ppid = getppid();
            write(p[1], &ppid, sizeof(ppid));
            close(p[1]);
            _exit(0);
        }
        /* 中间进程退出，不等孙 → 孙被 reparent */
        _exit(0);
    }
    close(p[1]);
    /* 等 mid 退出 */
    int st = 0;
    wait4(mid, &st, 0, NULL);

    pid_t new_ppid = 0;
    ssize_t r = read(p[0], &new_ppid, sizeof(new_ppid));
    close(p[0]);
    CHECK(r == (ssize_t)sizeof(new_ppid),
          "reparent: 读到 grandchild 写入的 ppid");
    CHECK(new_ppid != mid,
          "reparent: 孙进程的 ppid 不再是已退出的 mid");
    CHECK(new_ppid != parent_pid,
          "reparent: 孙进程的 ppid 也不是最初的父 (不同命名空间下本条可能松)");
}

/* ---------- PART 9: 深层覆盖 ---------- */

/* CLONE_THREAD 需要 CLONE_VM|CLONE_SIGHAND，单独 THREAD → EINVAL */
static void test_clone3_thread_needs_vm(void)
{
    SKIP_IF_NO_CLONE3("clone3 THREAD 组合检查");
    struct clone3_args ca = {0};
    ca.flags = CLONE_THREAD;
    ca.exit_signal = 0;

    errno = 0;
    long rc = syscall(__NR_clone3, &ca, sizeof(ca));
    if (rc == 0) {
        _exit(202);
    }
    CHECK(rc == -1 && errno == EINVAL,
          "clone3 CLONE_THREAD 缺 VM/SIGHAND → EINVAL");
}

/* CLONE_SIGHAND 必须带 CLONE_VM → EINVAL */
static void test_clone3_sighand_needs_vm(void)
{
    SKIP_IF_NO_CLONE3("clone3 SIGHAND 组合检查");
    struct clone3_args ca = {0};
    ca.flags = CLONE_SIGHAND;
    ca.exit_signal = SIGCHLD;

    errno = 0;
    long rc = syscall(__NR_clone3, &ca, sizeof(ca));
    if (rc == 0) {
        _exit(203);
    }
    CHECK(rc == -1 && errno == EINVAL,
          "clone3 CLONE_SIGHAND 缺 VM → EINVAL");
}

/* CLONE_THREAD + exit_signal != 0 非法 */
static void test_clone3_thread_exit_signal_einval(void)
{
    SKIP_IF_NO_CLONE3("clone3 THREAD+exit_signal 检查");
    struct clone3_args ca = {0};
    ca.flags = CLONE_THREAD | CLONE_VM | CLONE_SIGHAND;
    ca.exit_signal = SIGCHLD;  /* 非法 */

    errno = 0;
    long rc = syscall(__NR_clone3, &ca, sizeof(ca));
    if (rc == 0) {
        _exit(204);
    }
    CHECK(rc == -1 && errno == EINVAL,
          "clone3 CLONE_THREAD 不应带 exit_signal → EINVAL");
}

/* CLONE_PARENT_SETTID 将 tid 写入 parent_tid 指向的地址 */
static void test_clone3_parent_settid(void)
{
    SKIP_IF_NO_CLONE3("clone3 PARENT_SETTID");
    pid_t parent_tid_slot = -1;
    struct clone3_args ca = {0};
    ca.flags = CLONE_PARENT_SETTID;
    ca.parent_tid = (uint64_t)&parent_tid_slot;
    ca.exit_signal = SIGCHLD;

    long pid = syscall(__NR_clone3, &ca, sizeof(ca));
    if (pid == 0) {
        _exit(0);
    }
    CHECK(pid > 0, "clone3 PARENT_SETTID happy");
    int status = 0;
    wait4(pid, &status, 0, NULL);
    CHECK(parent_tid_slot > 0 && parent_tid_slot == (pid_t)pid,
          "CLONE_PARENT_SETTID: parent_tid 槽被写入 child tid");
}

/* 不加 CLONE_FILES 时父子 fd 表独立：父 close 后子 fd 仍有效 */
static void test_fork_fd_independent(void)
{
    unlink(TMPFILE);
    int fd = open(TMPFILE, O_CREAT | O_RDWR | O_TRUNC, 0644);
    CHECK(fd >= 0, "fd 独立: open TMPFILE");

    pid_t pid = fork();
    if (pid == 0) {
        /* 子：立刻等父关 fd，然后尝试写 */
        usleep(100 * 1000);
        ssize_t w = write(fd, "X", 1);
        _exit(w == 1 ? 0 : 1);
    }
    /* 父：关自己的 fd，不应影响子 */
    close(fd);

    int status = 0;
    wait4(pid, &status, 0, NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "fd 独立: 父 close 后子继续写 fd 成功");

    unlink(TMPFILE);
}

/* set_tid_address(NULL) 应成功，并仍返回当前 tid */
static void test_set_tid_address_null(void)
{
    long rc = syscall(SYS_set_tid_address, (void *)0);
    CHECK(rc == (long)gettid(),
          "set_tid_address(NULL) 返回 gettid()");
}

/* 退出码被截断到低 8 位：exit(256) 等价 exit(0) */
static void test_exit_code_truncation(void)
{
    pid_t pid = fork();
    if (pid == 0) {
        _exit(256);  /* 高 8 位被 kernel 丢弃 */
    }
    int status = 0;
    wait4(pid, &status, 0, NULL);
    CHECK(WIFEXITED(status), "exit(256): WIFEXITED");
    CHECK_RET(WEXITSTATUS(status), 0,
              "exit(256) 低 8 位为 0 → WEXITSTATUS == 0");
}

/* O_CLOEXEC fd 在 execve 后应被关闭 */
static void test_cloexec_across_execve(void)
{
    /* 用 pipe 做通道：父端保留读端，写端传给子 */
    int p[2];
    CHECK_RET(pipe(p), 0, "pipe 用于 CLOEXEC 验证");

    /* 把子端的写 fd 设为 O_CLOEXEC */
    int flags = fcntl(p[1], F_GETFD);
    CHECK(flags >= 0, "fcntl F_GETFD");
    CHECK(fcntl(p[1], F_SETFD, flags | FD_CLOEXEC) == 0,
          "fcntl F_SETFD FD_CLOEXEC");

    pid_t pid = fork();
    if (pid == 0) {
        /* execve 后 CLOEXEC fd 应被关。这里走自回环 --exec-ok。
         * 如果 CLOEXEC 未生效，子进程保有 p[1] 打开，父端 read 会阻塞。 */
        close(p[0]);
        char *argv[] = {(char *)self_path, (char *)"--exec-ok", NULL};
        char *envp[] = {NULL};
        execve(self_path, argv, envp);
        _exit(255);
    }
    /* 父端：关写端，然后 read；若 CLOEXEC 生效，pipe 会 EOF（0 字节） */
    close(p[1]);
    char buf[4];
    ssize_t r = read(p[0], buf, sizeof(buf));
    CHECK_RET(r, 0, "CLOEXEC: execve 后管道 EOF → read=0");

    int status = 0;
    wait4(pid, &status, 0, NULL);
    close(p[0]);
}

/* fork 后子进程的 gettid == getpid（新主线程） */
static void test_fork_child_tid_equals_pid(void)
{
    pid_t pid = fork();
    if (pid == 0) {
        if (getpid() != gettid()) {
            _exit(1);
        }
        _exit(0);
    }
    int status = 0;
    wait4(pid, &status, 0, NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "fork 子: gettid() == getpid()");
}

/* execve 保持 PID 不变 */
static void test_execve_preserves_pid(void)
{
    pid_t pid = fork();
    if (pid == 0) {
        pid_t me = getpid();
        char numbuf[16];
        snprintf(numbuf, sizeof(numbuf), "%d", me);
        char *argv[] = {(char *)self_path,
                        (char *)"--exec-check-pid",
                        numbuf, NULL};
        char *envp[] = {NULL};
        execve(self_path, argv, envp);
        _exit(255);
    }
    int status = 0;
    wait4(pid, &status, 0, NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "execve: 新映像 getpid() == 旧 pid");
}

/* wait4 status=NULL 也合法 */
static void test_wait4_null_status(void)
{
    pid_t pid = fork();
    if (pid == 0) {
        _exit(7);
    }
    pid_t w = wait4(pid, NULL, 0, NULL);
    CHECK_RET(w, pid, "wait4(pid, NULL, 0, NULL) 成功返回 pid");
}

/* clone3 CLONE_PIDFD：把 pidfd 写入 pidfd 字段指向的地址 */
static void test_clone3_pidfd(void)
{
    SKIP_IF_NO_CLONE3("clone3 CLONE_PIDFD");
    int pidfd_slot = -1;
    struct clone3_args ca = {0};
    ca.flags = CLONE_PIDFD;
    ca.pidfd = (uint64_t)&pidfd_slot;
    ca.exit_signal = SIGCHLD;

    long pid = syscall(__NR_clone3, &ca, sizeof(ca));
    if (pid == 0) {
        _exit(0);
    }
    CHECK(pid > 0, "clone3 CLONE_PIDFD happy");
    CHECK(pidfd_slot >= 0, "clone3 CLONE_PIDFD 写入 fd");

    int status = 0;
    wait4(pid, &status, 0, NULL);
    if (pidfd_slot >= 0) {
        close(pidfd_slot);
    }
}

/* ---------- main ---------- */

int main(int argc, char **argv)
{
    if (argc > 0 && argv[0] && argv[0][0] == '/') {
        strncpy(self_path, argv[0], sizeof(self_path) - 1);
        self_path[sizeof(self_path) - 1] = '\0';
    }

    int self_rc = handle_self_check(argc, argv);
    if (self_rc >= 0) {
        return self_rc;
    }

    TEST_START("fork/clone/clone3/execve/wait4/exit*/getpid/tid/... 语义");

    test_getpid_getppid_gettid();
    test_set_tid_address();
    test_fork_and_wait();
    test_fork_cow();
    test_fork_fd_inheritance();
    test_wait4_wnohang();
    test_wait4_any_multi_children();
    test_wait4_echild();
    test_wait4_unknown_pid();
    test_exit_code_passthrough();
    test_clone_files_share();
    test_clone3_basic();
    test_clone3_small_size_einval();
    test_clone3_detached_einval();
    test_execve_self_ok();
    test_execve_self_code();
    test_execve_enoent();
    test_execve_directory();
    test_reparent_on_parent_exit();

    /* 深层覆盖 */
    test_clone3_thread_needs_vm();
    test_clone3_sighand_needs_vm();
    test_clone3_thread_exit_signal_einval();
    test_clone3_parent_settid();
    test_fork_fd_independent();
    test_set_tid_address_null();
    test_exit_code_truncation();
    test_cloexec_across_execve();
    test_fork_child_tid_equals_pid();
    test_execve_preserves_pid();
    test_wait4_null_status();
    test_clone3_pidfd();

    TEST_DONE();
}
