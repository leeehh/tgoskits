#define _GNU_SOURCE
#include <errno.h>
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

int main(void) {
    printf("[TEST] sys_getcwd Linux contract (raw syscall)\n");

    /* 1) NULL buffer with non-zero size → EFAULT (kernel contract). */
    errno = 0;
    long r = syscall(SYS_getcwd, (char *)NULL, (unsigned long)4096);
    CHECK(r == -1 && errno == EFAULT,
          "getcwd(NULL, 4096) → -1/EFAULT");

    /* 2) Success path: return value equals strlen(buf)+1. */
    char buf[128];
    memset(buf, 0, sizeof(buf));
    errno = 0;
    r = syscall(SYS_getcwd, buf, (unsigned long)sizeof(buf));
    CHECK(r > 0, "getcwd(buf, 128) returns positive length");
    if (r > 0) {
        CHECK((size_t)r == strlen(buf) + 1,
              "return value equals strlen(cwd)+1");
    }

    /* 3) Too-small buffer → ERANGE. */
    errno = 0;
    r = syscall(SYS_getcwd, buf, (unsigned long)1);
    CHECK(r == -1 && errno == ERANGE,
          "getcwd(buf, 1) → -1/ERANGE");

    TEST_DONE();
}
