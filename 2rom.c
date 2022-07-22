#define _GNU_SOURCE
#include "endian.h"
#include <err.h>
#include <inttypes.h>
#include <iso646.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#if __STDC_VERSION__ >= 201112L
#include <stdnoreturn.h>
#else
#define noreturn
#endif
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include "hexdump.h"
#include "mapfile.h"
#include "types.h"
#include "buildrom.h"

extern char *__progname;
static void noreturn usage(void);

enum mode_e {
	MODE_IDK,
	MODE_LIST,
	MODE_EXTRACT,
	MODE_BUILD,
};

enum endianness_e {
	E_IDK,
	E_LITTLE,
	E_BIG,
};

void print_extinfo(struct extinfo_s *e)
{
	printf("\n%3d %3d %3x %3d\n",
		e->val1,
		e->val2,
		e->len,
		e->type
	);
	if (e->len > 0) {
		hexdump(e->data, e->len);
	}
}

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

int main(int argc, char *argv[])
{
	FILE *fdir = NULL;
	bool makedirtxt = true;
	char *infilename = NULL;
	char *outfilename = NULL;
	enum endianness_e endianness;
	enum mode_e mode = MODE_IDK;
	int ch;
	int verbose = 0;
	struct romdir_s *romdir;
	
	while ((ch = getopt(argc, argv, "lxbi:o:nv")) != -1)
		switch (ch) {
		case 'l':
			if (mode != MODE_IDK)
				usage();
			mode = MODE_LIST;
			break;
		case 'x':
			if (mode != MODE_IDK)
				usage();
			mode = MODE_EXTRACT;
			break;
		case 'b':
			if (mode != MODE_IDK)
				usage();
			mode = MODE_BUILD;
			break;
		case 'i':
			if (infilename)
				usage();
			infilename = optarg;
			break;
		case 'o':
			if (outfilename)
				usage();
			outfilename = optarg;
			break;
		case 'n':
			makedirtxt = false;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (*argv != NULL)
		usage();

	/*
	 *  check arguments for correctness
	 */
	switch (mode) {
	case MODE_LIST:
		if (!infilename)
			usage();
		if (verbose > 0)
			usage();
		fdir = stdout;
		break;
	case MODE_EXTRACT:
		if (!infilename)
			usage();
		if (verbose > 0)
			usage();
		if (makedirtxt) {
			fdir = fopen("dir.txt", "w");
			if (!fdir) err(1, "couldn't open \"%s\" for writing", "dir.txt");
		}
		break;
	case MODE_BUILD:
		if (!infilename)
			infilename = "dir.txt";
		if (!outfilename)
			outfilename = "out.rom";
		if (verbose > 2)
			usage();
		return buildrom(outfilename, infilename, verbose);
		break;
	default:
		usage();
		break;
	}

	struct MappedFile_s m = MappedFile_Open(infilename, false);
	if (!m.data) err(1, "couldn't open file");

	romdir = memmem(
		(void *) m.data,
		(size_t) m.size,
		(void *) "RESET",
		(size_t) 5	// note: not including null terminator
	);
	if (!romdir) errx(1, "couldn't find RESET");

	// check endianness of archive
	if (!memcmp(romdir, "RESET", 6)) { //including null terminator
		// little endian
		endianness = E_LITTLE;
	} else if (!memcmp(romdir, "RESETB", 7)) {
		// big endian
		endianness = E_BIG;
	} else {
		errx(1, "indeterminate endianness");
	}

	off_t offset = 0;
	off_t extinfo_offset = 0;
	void *extinfo_ptr = NULL;
	for (struct romdir_s *cur = romdir; cur->name[0]; cur++) {
		uint32_t file_size;
		char name[11] = {'\0'};
		memcpy(name, cur->name, 10);
		switch (endianness) {
		case E_BIG:
			file_size = be32toh(cur->size);
			break;
		case E_LITTLE:
		default:
			file_size = le32toh(cur->size);
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
	for(struct romdir_s *cur = romdir; cur->name[0]; cur++) {
		char name[11] = {'\0'};
		uint16_t extinfo_size;
		uint32_t file_size;
		struct MappedFile_s n;
		bool should_extract = true;
		bool should_display = true;

		memcpy(name, cur->name, 10);

		switch (endianness) {
		case E_BIG:
			extinfo_size = be16toh(cur->extinfo_size);
			file_size = be32toh(cur->size);
			break;
		case E_LITTLE:
		default:
			extinfo_size = le16toh(cur->extinfo_size);
			file_size = le32toh(cur->size);
			break;
		}

		if(!strcmp("-", name)) {
			should_extract = false;
			//should_display = false;
			uint8_t *filedata = (uint8_t *)(m.data + offset);
			for (size_t i = 0; i < file_size; i++) {
				if (filedata[i] != 0) {
					should_extract = true;
					should_display = true;
					break;
				}
			}
		}

		if (should_display && makedirtxt) fprintf(fdir, "%-10s     ", name);
		bool did_set_date = false;
		time_t mytime = 0;
		struct tm mytm = {0};
		for (size_t i = 0; i < extinfo_size;) {
			struct extinfo_s *e;
			uint32_t temp;
			e = (struct extinfo_s *)(extinfo_ptr + extinfo_offset + i);
			memcpy(&temp, e, 4);
			if (endianness == E_LITTLE)
				temp = le32toh(temp);
			else if (endianness == E_BIG)
				temp = be32toh(temp);
			else errx(1, "bad endianness");
			memcpy(e, &temp, 4);
			switch(e->type) {
			case ET_DATE:
				if (endianness == E_LITTLE) {
					mytm.tm_year = bcd2num(e->data[3])*100 - 1900;
					mytm.tm_year += bcd2num(e->data[2]);
					mytm.tm_mon = bcd2num(e->data[1]) - 1;
					mytm.tm_mday = bcd2num(e->data[0]);
				} else {
					mytm.tm_year = bcd2num(e->data[0])*100 - 1900;
					mytm.tm_year += bcd2num(e->data[1]);
					mytm.tm_mon = bcd2num(e->data[2]) - 1;
					mytm.tm_mday = bcd2num(e->data[3]);
				}
				mytime = mktime(&mytm);
				if (mytime != (time_t)-1)
					did_set_date = true;
				if (should_display && makedirtxt)
					fprintf(fdir,
						" %04d-%02d%02d",
						mytm.tm_year + 1900,
						mytm.tm_mon + 1,
						mytm.tm_mday
					);
				break;
			case ET_VERSION:
				uint8_t major;
				uint8_t minor;
				if (endianness == E_LITTLE) {
					major = e->val2;
					minor = e->val1;
				} else {
					major = e->val1;
					minor = e->val2;
				}

				if (should_display && makedirtxt)
					fprintf(fdir,
						" v%d.%d",
						major,
						minor
					);
				break;
			case ET_COMMENT:
				if (should_display && makedirtxt)
					fprintf(fdir, " \"%s\"", e->data);
				break;
			case ET_FIXEDADDR:
				if (should_display && makedirtxt)
					fprintf(fdir, " FIXEDADDR=0x%x", offset);
				break;
			default:
				fprintf(stderr, "\n unknown extinfo tag %d\n", e->type);
				print_extinfo(e);
				break;
			}
			i += sizeof(struct extinfo_s) + e->len;
		}
		if (should_display && makedirtxt) fprintf(fdir, "\n");

		if ((mode == MODE_EXTRACT) && should_extract) {
			if (!strcmp("-", name)) {
				snprintf(name, sizeof(name) - 1, "%08xh", offset);
				name[sizeof(name) - 1] = '\0';
				hexdump(m.data + offset, file_size);
			}
			n = MappedFile_Create(name, file_size);
			if (!n.data) err(1, "couldn't create file");
			memcpy(n.data, m.data + offset, file_size);
			MappedFile_Close(n);
			if (did_set_date) {
				struct utimbuf times = {.actime = mytime, .modtime = mytime};
				if (utime(name, &times)) err(1, "couldn't set time");
			}
		}

		offset += (file_size+15) & ~15;
		extinfo_offset += extinfo_size;
	}

	MappedFile_Close(m);

	return EXIT_SUCCESS;
}

static void noreturn usage(void)
{
	(void)fprintf(stderr, "usage: \t%1$s -l -i in.rom\n"
				"\t%1$s -x -i in.rom\n"
				"\t%1$s -b [-vv] [-i dir.txt] [-o out.rom]\n",
		__progname
	);
	exit(EXIT_FAILURE);
}
