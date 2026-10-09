/* Tiny I/O shim for the PIL runs: semihosting on the emulated MCU, stdio on the host.
 * (taken from github.com/pavanyadava007/motor-temp-edge, same author) */
#ifndef BMS_IO_H
#define BMS_IO_H
int io_open(const char *path, int for_write);
int io_read(int h, void *buf, int n); /* returns bytes read */
int io_write(int h, const void *buf, int n);
void io_close(int h);
void io_puts(const char *s);
void io_exit(int code);
int pil_main(void);
#endif
