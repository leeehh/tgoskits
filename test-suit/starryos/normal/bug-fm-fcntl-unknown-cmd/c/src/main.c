#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
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
    printf("[TEST] sys_fcntl reject unknown cmd\n");
    int fd = 0; /* stdin is always valid */

    /* 0x5EA1 is well outside any defined F_* cmd */
    errno = 0;
    int r = fcntl(fd, 0x5EA1, 0);
    CHECK(r == -1 && errno == EINVAL, "fcntl(0, 0x5EA1) → -1/EINVAL");

    /* sanity: a known cmd still works */
    errno = 0;
    int flags = fcntl(fd, F_GETFD);
    CHECK(flags != -1, "fcntl(0, F_GETFD) succeeds");

    TEST_DONE();
}
