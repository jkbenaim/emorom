#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "hexdump.h"

void hexdump(const void *buf, size_t siz)
{
	const uint8_t *mybuf = buf;
	const char addrstr[] = "%8zx    ";
	size_t off = 0;
	while (siz) {
		unsigned numbytes;
		size_t i;
		numbytes = (siz>16)?16:siz;
		printf(addrstr, off);
		for (i = 0; i < numbytes; i++) {
			printf("%02x ", mybuf[i]);
		}
		for (i = numbytes; i < 16; i++) {
			printf("   ");
		}
		printf("   ");
		for (i = 0; i < numbytes; i++) {
			putchar(isprint(mybuf[i])?mybuf[i]:'.');
		}
		printf("\n");
		siz -= numbytes;
		mybuf += numbytes;
		off += 16;
	}
}

