#define _GNU_SOURCE
#include <endian.h>
#include <err.h>
#include <inttypes.h>
#include <iso646.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include "mapfile.h"

extern char *__progname;
static void noreturn usage(void);

int main(int argc, char *argv[])
{
	char *filename = NULL;
	int ch;
	
	while ((ch = getopt(argc, argv, "f:")) != -1)
		switch (ch) {
		case 'f':
			if (filename)
				usage();
			filename = optarg;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (not filename)
		usage();
	if (*argv != NULL)
		usage();


	return EXIT_SUCCESS;
}

static void noreturn usage(void)
{
	(void)fprintf(stderr, "usage: \t%1$s -l -f romimg\n"
				"\t%1$s -x -f romimg\n",
		__progname
	);
	exit(EXIT_FAILURE);
}

enum endianness_e {
	E_IDK,
	E_LITTLE,
	E_BIG,
};

struct __attribute__((packed)) romdir_e {
	char name[10];
	uint16_t ext_info_size;
	uint32_t file_size;
};

struct __attribute__((packed)) extinfo_entry_s {
	uint8_t val1;
	uint8_t val2;
	uint8_t len;
	uint8_t type;
	uint8_t data[];
};

enum extinfo_types_e {
	EXTINFO_TYPE_DATE = 1,
	EXTINFO_TYPE_VERSION,
	EXTINFO_TYPE_COMMENT,
	EXTINFO_TYPE_NULL = 127,
};

int bcd2num(unsigned bcd)
{
	unsigned hi, lo;

	if (bcd > 0x99) return -1;
	if ((bcd & 0x0f) > 0x09) return -1;
	if ((bcd & 0xf0) > 0x90) return -1;

	lo = bcd & 0x0f;
	hi = bcd & 0xf0;

	return lo + (hi/16)*10;
}

int num2bcd(unsigned num)
{
	if (num > 99) return -1;

	return (num % 10) + (num/10)*16;
}

int oldmain(int argc, char *argv[])
{
	__label__ out_return, out_close;
	char *zErr = NULL;
	struct romdir_e *romdir;
	enum endianness_e endianness;

	if (argc < 2) {
		usage();
		return 1;
	}

	struct MappedFile_s m = MappedFile_Open(argv[1], false);
	if (!m.data) {
		zErr = "couldn't open file";
		goto out_return;
	}

	romdir = memmem(
		(void *) m.data,
		(size_t) m.size,
		(void *) "RESET",
		(size_t) 5	// note: not including null terminator
	);
	if (!romdir) {
		zErr = "couldn't find RESET";
		goto out_close;
	}

	// check endianness of archive
	if (!memcmp(romdir, "RESET", 6)) {
		// little endian
		endianness = E_LITTLE;
	} else if (!memcmp(romdir, "RESETB", 7)) {
		// big endian
		endianness = E_BIG;
	} else {
		zErr = "indeterminate endianness";
		goto out_close;
	}

	off_t offset = 0;
	off_t extinfo_offset = 0;
	void *extinfo_ptr = NULL;
	for (struct romdir_e *cur = romdir; cur->name[0]; cur++) {
		uint32_t file_size;
		char name[11] = {'\0'};
		memcpy(name, cur->name, 10);
		switch (endianness) {
		case E_BIG:
			file_size = be32toh(cur->file_size);
			break;
		case E_LITTLE:
		default:
			file_size = le32toh(cur->file_size);
			break;
		}
		if (!strcmp("EXTINFO", name)) {
			extinfo_ptr = m.data + offset;
			break;
		}
		offset += (file_size+15) & ~15;
	}
	offset = 0;
	bool last_file_was_padding = false;
	for(struct romdir_e *cur = romdir; cur->name[0]; cur++) {
		char name[11] = {'\0'};
		uint16_t extinfo_size;
		uint32_t file_size;
		struct MappedFile_s n;

		memcpy(name, cur->name, 10);
		switch (endianness) {
		case E_BIG:
			extinfo_size = be16toh(cur->ext_info_size);
			file_size = be32toh(cur->file_size);
			break;
		case E_LITTLE:
		default:
			extinfo_size = le16toh(cur->ext_info_size);
			file_size = le32toh(cur->file_size);
			break;
		}
		printf("0x%08x %10s", offset, name);
		void *thingy = extinfo_ptr + extinfo_offset;
		bool did_set_date = false;
		time_t mytime = 0;
		struct tm mytm = {0};
		for (size_t i = 0; i < extinfo_size;) {
			struct extinfo_entry_s *e;
			e = (struct extinfo_entry_s *)(extinfo_ptr + extinfo_offset + i);
			switch(e->type) {
			case EXTINFO_TYPE_DATE:
				printf(" %02x%02x-%02x%02x",
					e->data[3],
					e->data[2],
					e->data[1],
					e->data[0]
				);
				mytm.tm_year = bcd2num(e->data[3])*100 - 1900;
				mytm.tm_year += bcd2num(e->data[2]);
				mytm.tm_mon = bcd2num(e->data[1]) - 1;
				mytm.tm_mday = bcd2num(e->data[0]);
				mytime = mktime(&mytm);
				if (mytime != (time_t)-1)
					did_set_date = true;
				break;
			case EXTINFO_TYPE_VERSION:
				printf(" v%d.%d",
					e->val2,
					e->val1
				);
				break;
			case EXTINFO_TYPE_COMMENT:
				printf("\t%s", e->data);
				break;
			default:
				break;
			}
			i += sizeof(struct extinfo_entry_s) + e->len;
		}
		printf("\n");

		if(!strcmp("-", name)) {
			last_file_was_padding = true;
		} else {
			n = MappedFile_Create(name, file_size);
			if (!n.data) continue;
			memcpy(n.data, m.data + offset, file_size);
			MappedFile_Close(n);
			if (did_set_date) {
				struct utimbuf times = {.actime = mytime, .modtime = mytime};
				if (utime(name, &times)) err(1, "couldn't set time");
			}
			if ((offset & 0xfff) == 0) {
				printf("^^ fixed position\n");
				last_file_was_padding = false;
			}
		}

		offset += (file_size+15) & ~15;
		extinfo_offset += extinfo_size;
	}

out_close:
	MappedFile_Close(m);
out_return:
	if (zErr) {
		fprintf(stderr, "error: %s\n", zErr);
		return 1;
	} else {
		return 0;
	}
}
