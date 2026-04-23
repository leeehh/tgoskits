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

#ifndef SYS_renameat2
#  if defined(__x86_64__)
#    define SYS_renameat2 316
#  elif defined(__aarch64__) || defined(__riscv)
#    define SYS_renameat2 276
#  endif
#endif

int main(void) {
    printf("[TEST] sys_renameat2 rejects unknown flags\n");
    int fd = open("/tmp/renameat2_probe_a", O_CREAT | O_WRONLY | O_TRUNC, 0600);
    CHECK(fd >= 0, "create source");
    close(fd);

    errno = 0;
    long r = syscall(SYS_renameat2, AT_FDCWD, "/tmp/renameat2_probe_a",
                     AT_FDCWD, "/tmp/renameat2_probe_b", 0xDEAD);
    CHECK(r == -1 && errno == EINVAL,
          "renameat2(.., 0xDEAD) → -1/EINVAL");

    /* cleanup */
    unlink("/tmp/renameat2_probe_a");
    unlink("/tmp/renameat2_probe_b");
    TEST_DONE();
}
