#ifndef _TYPES_H_
#define _TYPES_H_
#include <stdint.h>
#include <stdbool.h>

#define MAXCOMMENTSIZE (250)

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
	ET_DATE = 1,
	ET_VERSION,
	ET_COMMENT,
	ET_FIXEDADDR = 127,
};

struct romlump_s {
	struct romlump_s *next;
	struct romlump_s *prev;
	void *data;
	char name[10];	// possibly without null terminator
	unsigned size;
	uint8_t date[4];
	uint8_t version[2];
	unsigned char comment[260];
	unsigned fixedaddr;
	bool date_present;
	bool version_present;
	bool comment_present;
	bool fixedaddr_present;
	uint16_t extinfo_size;
};
#endif

