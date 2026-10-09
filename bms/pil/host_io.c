/* Host side of the I/O shim (stdio). */
#include <stdio.h>
#include <stdlib.h>
#include "bms_io.h"

static FILE *files[8];

int io_open(const char *path, int for_write)
{
	int i;

	for (i = 0; i < 8; i++) {
		if (files[i] == NULL) {
			files[i] = fopen(path, (for_write != 0) ? "wb" : "rb");
			return (files[i] != NULL) ? i : -1;
		}
	}
	return -1;
}

int io_read(int h, void *buf, int n)
{
	return (int)fread(buf, 1u, (size_t)n, files[h]);
}

int io_write(int h, const void *buf, int n)
{
	return (int)fwrite(buf, 1u, (size_t)n, files[h]);
}

void io_close(int h)
{
	(void)fclose(files[h]);
	files[h] = NULL;
}

void io_puts(const char *s)
{
	(void)fputs(s, stdout);
}

void io_exit(int code)
{
	exit(code);
}

int main(void)
{
	return pil_main();
}
