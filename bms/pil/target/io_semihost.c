/* ARM semihosting (BKPT 0xAB) I/O. Paths are relative to QEMU's working directory. */
#include <stdint.h>
#include "bms_io.h"

static int sh_call(int op, void *arg)
{
    register int r0 __asm__("r0") = op;
    register void *r1 __asm__("r1") = arg;
    __asm volatile("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory");
    return r0;
}

static int str_len(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

int io_open(const char *path, int for_write)
{
    uint32_t a[3];
    a[0] = (uint32_t)path;
    a[1] = for_write ? 5u : 1u; /* "wb" : "rb" */
    a[2] = (uint32_t)str_len(path);
    return sh_call(0x01, a);
}

int io_read(int h, void *buf, int n)
{
    uint32_t a[3];
    a[0] = (uint32_t)h;
    a[1] = (uint32_t)buf;
    a[2] = (uint32_t)n;
    return n - sh_call(0x06, a); /* SYS_READ returns bytes NOT read */
}

int io_write(int h, const void *buf, int n)
{
    uint32_t a[3];
    a[0] = (uint32_t)h;
    a[1] = (uint32_t)buf;
    a[2] = (uint32_t)n;
    return n - sh_call(0x05, a);
}

void io_close(int h)
{
    uint32_t a[1];
    a[0] = (uint32_t)h;
    sh_call(0x02, a);
}

void io_puts(const char *s) { sh_call(0x04, (void *)s); }

void io_exit(int code)
{
    /* ADP_Stopped_ApplicationExit -> QEMU exit 0, RunTimeErrorUnknown -> exit 1 */
    sh_call(0x18, (void *)(code == 0 ? 0x20026 : 0x20023));
    for (;;) {
    }
}
