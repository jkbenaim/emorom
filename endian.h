#ifndef _ENDIAN_H_
#define _ENDIAN_H_

/*
2rom.o: In function `main':
2rom.c(.text+0x7cc): undefined reference to `be32toh'
2rom.c(.text+0x7f8): undefined reference to `le32toh'
2rom.c(.text+0x928): undefined reference to `be16toh'
2rom.c(.text+0x950): undefined reference to `be32toh'
2rom.c(.text+0x984): undefined reference to `le16toh'
2rom.c(.text+0x9ac): undefined reference to `le32toh'
buildrom.o: In function `make_romdir':
buildrom.c(.text+0xb14): undefined reference to `htole16'
buildrom.c(.text+0xb74): undefined reference to `htole32'
buildrom.c(.text+0xbdc): undefined reference to `htole16'
buildrom.c(.text+0xc40): undefined reference to `htole32'
collect2: ld returned 1 exit status
*/

#include <byteswap.h>
#include <endian.h>

#if __BYTE_ORDER == __BIG_ENDIAN
#define htole16(x) bswap_16(x)
#define htole32(x) bswap_32(x)
#define le16toh(x) bswap_16(x)
#define le32toh(x) bswap_32(x)
#define htobe16(x) ((uint16_t)x)
#define htobe32(x) ((uint32_t)x)
#define be16toh(x) ((uint16_t)x)
#define be32toh(x) ((uint32_t)x)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define htole16(x) ((uint16_t)x)
#define htole32(x) ((uint32_t)x)
#define le16toh(x) ((uint16_t)x)
#define le32toh(x) ((uint32_t)x)
#define htobe16(x) bswap_16(x)
#define htobe32(x) bswap_32(x)
#define be16toh(x) bswap_16(x)
#define be32toh(x) bswap_32(x)
#endif

#endif
