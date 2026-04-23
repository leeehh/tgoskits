#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

/* ===== inline test framework ===== */
static int g_passed, g_failed;
#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        printf("  [FAIL] %s (errno=%d %s)\n", (msg), errno, strerror(errno)); \
        g_failed++; \
    } else { \
        printf("  [OK] %s\n", (msg)); \
        g_passed++; \
    } \
} while (0)
#define TEST_DONE() do { \
    printf("\n=== %d passed, %d failed ===\n", g_passed, g_failed); \
    if (g_failed == 0) printf("TEST PASSED\n"); \
    return g_failed == 0 ? 0 : 1; \
} while (0)

#ifndef SYS_copy_file_range
#  if defined(__x86_64__)
#    define SYS_copy_file_range 326
#  elif defined(__aarch64__) || defined(__riscv)
#    define SYS_copy_file_range 285
#  endif
#endif

int main(void) {
    printf("[TEST] sys_copy_file_range rejects unknown flags\n");
    const char *src = "/tmp/cfr_src", *dst = "/tmp/cfr_dst";
    int fd_src = open(src, O_CREAT | O_RDWR | O_TRUNC, 0600);
    CHECK(fd_src >= 0, "create src");
    write(fd_src, "abc", 3);
    lseek(fd_src, 0, SEEK_SET);
    int fd_dst = open(dst, O_CREAT | O_RDWR | O_TRUNC, 0600);
    CHECK(fd_dst >= 0, "create dst");

    errno = 0;
    long r = syscall(SYS_copy_file_range, fd_src, NULL, fd_dst, NULL, 3, 0xDEAD);
    CHECK(r == -1 && errno == EINVAL,
          "copy_file_range(..., 0xDEAD) → -1/EINVAL");

    close(fd_src);
    close(fd_dst);
    unlink(src); unlink(dst);
    TEST_DONE();
}
