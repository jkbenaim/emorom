%option noyywrap
%{
#include <err.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "hexdump.h"
#include "mapfile.h"
#include "types.h"

struct romlump_s *head = NULL;
struct romlump_s *tail = NULL;

void new_lump()
{
	struct romlump_s *newlump = calloc(1, sizeof(struct romlump_s));
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
	struct romlump_s *temp;
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
	struct romlump_s *lump = head;
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

struct romlump_s *get_lump_for_name(const char *name)
{
	struct romlump_s *cur = head;
	while (cur) {
		if (!strncmp(cur->name, name, sizeof(cur->name)))
			return cur;
		cur = cur->next;
	}
	return NULL;
}

void make_extinfo()
{
	struct romlump_s *extinfo;
	struct romlump_s *cur;
	size_t offset = 0;

	extinfo = get_lump_for_name("EXTINFO");
	if (!extinfo)
		errx(1, "no EXTINFO lump");

	extinfo->size = 0;
	free(extinfo->data);
	extinfo->data = malloc(extinfo->size);
	if (!extinfo->data)
		err(1, "malloc failure");

	struct extinfo_entry_s *extend(struct romlump_s *lump, size_t size)
	{
		struct extinfo_entry_s *e = NULL;
		// round up to multiple of 4
		size = (size + 3) & ~3;
		size += sizeof(struct extinfo_entry_s);
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
		struct extinfo_entry_s *e;
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
%}
%%
[\t ]+

^[^\t ]+ {
	struct MappedFile_s m = {0};
	//printf("filename:\t%s\n", yytext);
	
	m = MappedFile_Open(yytext, false);
	if (!m.data) err(1, "couldn't open file for reading");
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
. { printf("IDKIDKIDK\n"); }
\n { }
%%
int main(int argc, char *argv[])
{
	yylex();
	//print_lumps();
	make_extinfo();

	struct romlump_s *extinfo;
	extinfo = get_lump_for_name("EXTINFO");
	if (!extinfo) errx(1, "couldn't find extinfo");
	struct MappedFile_s m = MappedFile_Create("myextinfo.bin", extinfo->size);
	if (!m.data) err(1, "couldn't open file for writing");
	memcpy(m.data, extinfo->data, extinfo->size);
	MappedFile_Close(m);
	m.data = NULL;
	hexdump(extinfo->data, extinfo->size);
	free_lumps();
	return EXIT_SUCCESS;
}
