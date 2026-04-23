#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/signalfd.h>
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
    printf("[TEST] sys_signalfd4 ignores CLOEXEC on update\n");
    sigset_t m1, m2;
    sigemptyset(&m1); sigaddset(&m1, SIGUSR1);
    sigemptyset(&m2); sigaddset(&m2, SIGUSR2);

    int sfd = signalfd(-1, &m1, SFD_NONBLOCK);
    CHECK(sfd >= 0, "create signalfd");

    errno = 0;
    int r = signalfd(sfd, &m2, SFD_CLOEXEC);
    /* Expected: r == sfd (update succeeds; CLOEXEC silently ignored). */
    CHECK(r == sfd, "signalfd(sfd, &mask, SFD_CLOEXEC) returns sfd");

    if (sfd >= 0) close(sfd);
    TEST_DONE();
}
