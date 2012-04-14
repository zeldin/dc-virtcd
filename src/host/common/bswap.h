#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif

#ifdef HAVE_DECL___BUILTIN_BSWAP32
#define SWAP32 __builtin_bswap32
#else
#ifdef HAVE_DECL_BSWAP_32
#define SWAP32 bswap_32
#else
#define SWAP16(n) (((uint16_t)((n)<<8))|(((uint16_t)(n))>>8))
#define SWAP32(n) ((SWAP16((uint32_t)(n))<<16)|SWAP16(((uint32_t)(n))>>16))
#endif
#endif
