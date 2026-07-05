/* Shim for FFmpeg's libavcodec/mathops.h — only the zigzag scan table is used. */
#ifndef AVCODEC_MATHOPS_H
#define AVCODEC_MATHOPS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint8_t ff_zigzag_direct[64];
extern const uint8_t ff_reverse[256];

static inline uint32_t bitswap_32(uint32_t x)
{
	return (uint32_t) ff_reverse[x & 0xFF] << 24 | (uint32_t) ff_reverse[(x >> 8) & 0xFF] << 16 |
		   (uint32_t) ff_reverse[(x >> 16) & 0xFF] << 8 | (uint32_t) ff_reverse[x >> 24];
}

#ifdef __cplusplus
}
#endif

#endif /* AVCODEC_MATHOPS_H */
