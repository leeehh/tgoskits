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
    printf("[TEST] sys_faccessat2 rejects invalid mode bits\n");
    const char *path = "/tmp/faccess_probe";
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    CHECK(fd >= 0, "create target");
    close(fd);

    errno = 0;
    /* 0xFF contains bits outside R_OK|W_OK|X_OK|F_OK */
    int r = faccessat(AT_FDCWD, path, 0xFF, 0);
    CHECK(r == -1 && errno == EINVAL,
          "faccessat(.., 0xFF, 0) → -1/EINVAL");

    unlink(path);
    TEST_DONE();
}
