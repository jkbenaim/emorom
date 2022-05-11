#ifndef _ENDIAN_H_
#define _ENDIAN_H_

#include <byteswap.h>
#include <endian.h>

#ifndef htole16
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

#endif
