%option noyywrap
%{
#include "endian.h"
#include <err.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "buildrom.h"
#include "hexdump.h"
#include "mapfile.h"
#include "types.h"

struct lump_s *head = NULL;
struct lump_s *tail = NULL;

void new_lump()
{
	struct lump_s *newlump = calloc(1, sizeof(struct lump_s));
	if (!newlump) err(1, "malloc failure");

	if (tail) {
		tail->next = newlump;
		newlump->prev = tail;
	}
	tail = newlump;
	if (!head) head = tail;
}

void free_lumps()
{
	struct lump_s *temp;
	while (head) {
		temp = head->next;
		head->next = NULL;
		head->prev = NULL;
		free(head->data);
		head->data = NULL;
		free(head);
		head = temp;
	}
}

void print_lumps()
{
	struct lump_s *lump = head;
	while (lump) {
		printf("%-10s", lump->name);
		if (lump->date_present)
			printf(" %02x%02x-%02x%02x",
				lump->date[3],
				lump->date[2],
				lump->date[1],
				lump->date[0]
			);
		if (lump->version_present)
			printf(" v%d.%d",
				lump->version[1],
				lump->version[0]
			);
		if (lump->fixedaddr_present)
			printf(" FIXEDADDR=0x%x",
				lump->fixedaddr
			);
		if (lump->comment_present)
			printf(" \"%s\"", lump->comment);
		printf("\n");
		lump = lump->next;
	}
}

int count_lumps()
{
	struct lump_s *lump = head;
	int numFiles = 0;
	while (lump) {
		numFiles++;
		lump = lump->next;
	}
	return numFiles;
}

struct lump_s *get_lump_for_name(const char *name)
{
	struct lump_s *cur = head;
	while (cur) {
		if (!strncmp(cur->name, name, sizeof(cur->name)))
			return cur;
		cur = cur->next;
	}
	return NULL;
}

void make_extinfo()
{
	struct lump_s *extinfo;
	struct lump_s *cur;
	size_t offset = 0;

	extinfo = get_lump_for_name("EXTINFO");
	if (!extinfo)
		errx(1, "no EXTINFO lump");

	extinfo->size = 0;
	free(extinfo->data);
	extinfo->data = malloc(extinfo->size);
	if (!extinfo->data)
		err(1, "malloc failure");

	struct extinfo_s *extend(struct lump_s *lump, size_t size)
	{
		struct extinfo_s *e = NULL;
		// round up to multiple of 4
		size = (size + 3) & ~3;
		size += sizeof(struct extinfo_s);
		lump->extinfo_size += size;
		extinfo->data = realloc(extinfo->data, extinfo->size + size);
		if (!extinfo->data)
			errx(1, "realloc failure");
		extinfo->size += size;
		e = extinfo->data + extinfo->size - size;
		memset(e, 0, size);
		e->len = size - 4;
		return e;
	}
	
	cur = head;
	while (cur) {
		struct extinfo_s *e;
		if (cur->date_present) {
			e = extend(cur, 4);
			e->type = ET_DATE;
			e->data[0] = cur->date[0];
			e->data[1] = cur->date[1];
			e->data[2] = cur->date[2];
			e->data[3] = cur->date[3];
		}
		if (cur->version_present) {
			e = extend(cur, 0);
			e->type = ET_VERSION;
			e->val1 = cur->version[0];
			e->val2 = cur->version[1];
		}
		if (cur->comment_present) {
			e = extend(cur, strlen(cur->comment)+1);
			e->type = ET_COMMENT;
			strcpy(e->data, cur->comment);
		}
		if (cur->fixedaddr_present) {
			e = extend(cur, 0);
			e->type = ET_FIXEDADDR;
		}
		cur = cur->next;
	}
}

void make_romdir()
{
	struct lump_s *romdir = NULL;
	struct lump_s *cur = NULL;

	romdir = get_lump_for_name("ROMDIR");
	if (!romdir)
		errx(1, "no ROMDIR lump");
	
	cur = head;
	size_t num_files = 0;
	while (cur) {
		num_files++;
		if (cur->fixedaddr_present) {
			// we will need to add a padding file
			num_files++;
		}
		cur = cur->next;
	}

	romdir->size = num_files * sizeof(struct romdir_s);
	free(romdir->data);
	romdir->data = calloc(num_files, sizeof(struct romdir_s));
	if (!romdir->data)
		err(1, "calloc failure");
	
	struct romdir_s *dir = romdir->data;
	cur = head;
	size_t size_so_far = 0;
	while (cur) {
		if (cur->fixedaddr_present && (cur->fixedaddr != size_so_far)) {
			// insert a padding file in the romdir
			size_t padsize = cur->fixedaddr - size_so_far;
			if (cur->fixedaddr < size_so_far)
				errx(1, "FIXEDADDR directive overwrites existing file");
			strcpy(dir->name, "-");
			dir->extinfo_size = htole16(0);
			dir->size = htole32(padsize);
			size_so_far += padsize;
			dir++;
		}
		strncpy(dir->name, cur->name, sizeof(dir->name));
		dir->extinfo_size = htole16(cur->extinfo_size);
		dir->size = htole32(cur->size);
		dir++;
		size_so_far += (cur->size + 15) & ~15;
		cur = cur->next;
	}
}

void make_rom(void **data, size_t *size)
{
	struct lump_s *romdir = NULL;
	size_t offset = 0;
	*data = NULL;
	*size = 0;

	romdir = get_lump_for_name("ROMDIR");
	if (!romdir) errx(1, "no ROMDIR lump");

	for (struct romdir_s *cur = romdir->data; cur->name[0]; cur++) {
		*size += (cur->size + 15) & ~15;
	}

	*data = calloc(1, *size);
	if (!*data) err(1, "calloc failure");

	for (struct lump_s *cur = head; cur; cur = cur->next) {
		if (cur->fixedaddr_present && (cur->fixedaddr != offset)) {
			size_t padsize = cur->fixedaddr - offset;
			offset += padsize;
		}
		memcpy(*data + offset, cur->data, cur->size);
		offset += (cur->size + 15) & ~15;
	}
}

%}
%%
[\t ]+

^[^\t -][^\t \n]* {
	struct MappedFile_s m = {0};
	//printf("filename:\t%s\n", yytext);
	m = MappedFile_Open(yytext, false);
	if (!m.data) err(1, "couldn't open file \"%s\" for reading", yytext);
	new_lump();
	strncpy(tail->name, yytext, sizeof(tail->name));
	tail->size = m.size;
	tail->data = malloc(m.size);
	if (!tail->data) err(1, "malloc failure");
	memcpy(tail->data, m.data, m.size);
	MappedFile_Close(m);
}

[0-9]{4}-[0-9]{4} {
	//printf("\tdate:\t\t%s\n", yytext);
	uint8_t text2bcd(unsigned char a, unsigned char b)
	{
		if ((a < '0') || (a > '9')) return ~0;
		if ((b < '0') || (b > '9')) return ~0;
		a = a - '0';
		b = b - '0';
		a *= 16;
		return a + b;
	}

	tail->date[3] = text2bcd(yytext[0], yytext[1]);
	tail->date[2] = text2bcd(yytext[2], yytext[3]);
	// skip the dash
	tail->date[1] = text2bcd(yytext[5], yytext[6]);
	tail->date[0] = text2bcd(yytext[7], yytext[8]);
	tail->date_present = true;
}

v[0-9]+\.[0-9]+ {
	//printf("\tversion:\t%s\n", yytext);
	char *major = yytext + 1;
	char *minor = index(yytext, '.') + 1;
	tail->version[1] = strtol(major, NULL, 10);
	tail->version[0] = strtol(minor, NULL, 10);
	tail->version_present = true;
}

\".*\" {
	size_t siz;
	//printf("\tcomment:\t%s\n", yytext);
	siz = strlen(yytext);
	if (siz > MAXCOMMENTSIZE)
		errx(1, "comment too large");
	strncpy(tail->comment, yytext + 1, sizeof(tail->comment));
	// remove final quote mark
	tail->comment[siz-2] = '\0';
	tail->comment_present = true;
}

FIXEDADDR=0x[0-9a-fA-F]+ {
	long addr;
	//printf("\tfixedaddr:\t%s\n", yytext);
	tail->fixedaddr = strtol(yytext + strlen("FIXEDADDR=0x"), NULL, 16);
	tail->fixedaddr_present = true;
}

^-.* {
	// this is a comment.
	// do nothing.
}

. { printf("IDKIDKIDK %s\n", yytext); }
\n { }
%%
int buildrom(const char *romname, const char *listingname, int verbose)
{
	FILE *listing = NULL;
	struct lump_s *extinfo = {0};
	struct MappedFile_s m = {0};
	struct lump_s *romdir = {0};
	void *data = NULL;
	size_t size = 0;

	listing = fopen(listingname, "r");
	if (!listing) err(1, "couldn't open listing file\"%s\" for reading", listingname);
	yyin = listing;
	yylex();
	//print_lumps();
	make_extinfo();
	make_romdir();

	/*
	extinfo = get_lump_for_name("EXTINFO");
	if (!extinfo) errx(1, "couldn't find extinfo");
	m = MappedFile_Create("myextinfo.bin", extinfo->size);
	if (!m.data) err(1, "couldn't open file \"%s\" for writing", "myextinfo.bin");
	memcpy(m.data, extinfo->data, extinfo->size);
	MappedFile_Close(m);
	m.data = NULL;

	romdir = get_lump_for_name("ROMDIR");
	if (!romdir) errx(1, "couldn't find romdir");
	m = MappedFile_Create("myromdir.bin", romdir->size);
	if (!m.data) err(1, "couldn't open file \"%s\" for writing", "myromdir.bin");
	memcpy(m.data, romdir->data, romdir->size);
	MappedFile_Close(m);
	m.data = NULL;
	*/

	make_rom(&data, &size);
	m = MappedFile_Create(romname, size);
	if (!m.data) err(1, "couldn't open file \"%s\" for writing", romname);
	memcpy(m.data, data, size);
	MappedFile_Close(m);
	m.data = NULL;
	free(data);
	data = NULL;
	if (verbose >= 2) {
		print_lumps();
	}
	if (verbose >= 1) {
		warnx("built \"%s\": %d files, %lu bytes",
			romname,
			count_lumps(),
			size
		);
	}

	free_lumps();
	return EXIT_SUCCESS;
}

