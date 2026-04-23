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
    printf("[TEST] sys_pipe2 rejects unknown flag bits\n");
    int fds[2] = { -1, -1 };
    errno = 0;
    int r = pipe2(fds, 0xDEAD);
    CHECK(r == -1 && errno == EINVAL,
          "pipe2(fds, 0xDEAD) → -1/EINVAL");

    /* clean up if it did create anything */
    if (fds[0] >= 0) close(fds[0]);
    if (fds[1] >= 0) close(fds[1]);
    TEST_DONE();
}
