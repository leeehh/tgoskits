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
    printf("[TEST] sys_linkat rejects unknown flag bits\n");
    const char *src = "/tmp/linkat_src", *dst = "/tmp/linkat_dst";
    unlink(dst);
    int fd = open(src, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    CHECK(fd >= 0, "create source");
    close(fd);

    errno = 0;
    int r = linkat(AT_FDCWD, src, AT_FDCWD, dst, 0xDEAD);
    CHECK(r == -1 && errno == EINVAL,
          "linkat(.., 0xDEAD) → -1/EINVAL");

    struct stat st;
    CHECK(stat(dst, &st) == -1 && errno == ENOENT,
          "dst must not exist after rejected linkat");

    unlink(src);
    unlink(dst);
    TEST_DONE();
}
