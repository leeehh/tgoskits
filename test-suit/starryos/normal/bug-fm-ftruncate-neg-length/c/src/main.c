#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
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
    printf("[TEST] sys_ftruncate rejects negative length\n");
    const char *path = "/tmp/ftrunc_probe";
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0600);
    CHECK(fd >= 0, "open target file");
    if (fd < 0) TEST_DONE();

    errno = 0;
    int r = ftruncate(fd, -1);
    CHECK(r == -1 && errno == EINVAL,
          "ftruncate(fd, -1) → -1/EINVAL");

    /* sanity: zero-length truncate works */
    errno = 0;
    r = ftruncate(fd, 0);
    CHECK(r == 0, "ftruncate(fd, 0) → 0");

    close(fd);
    unlink(path);
    TEST_DONE();
}
