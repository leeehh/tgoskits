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
    printf("[TEST] sys_fadvise64 rejects bad fd with EBADF\n");

    errno = 0;
    int r = posix_fadvise(-1, 0, 0, POSIX_FADV_NORMAL);
    /* Linux convention: posix_fadvise returns errno on error, not -1 */
    CHECK(r == EBADF,
          "posix_fadvise(-1, ..., NORMAL) returns EBADF");

    /* sanity: on a valid fd, NORMAL should succeed */
    int fd = open("/tmp/fadvise_probe", O_CREAT | O_RDWR, 0600);
    CHECK(fd >= 0, "open /tmp/fadvise_probe");
    if (fd >= 0) {
        r = posix_fadvise(fd, 0, 0, POSIX_FADV_NORMAL);
        CHECK(r == 0, "posix_fadvise(valid, NORMAL) returns 0");
        close(fd);
        unlink("/tmp/fadvise_probe");
    }
    TEST_DONE();
}
