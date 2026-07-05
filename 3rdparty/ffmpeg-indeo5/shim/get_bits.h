/* Shim for FFmpeg's libavcodec/get_bits.h — little-endian bitstream reader only
 * (the vendored Indeo sources define BITSTREAM_READER_LE), semantics matched to
 * FFmpeg n4.4.4: LSB-first within little-endian storage, reads of up to 25 bits in
 * one step, and the two-entry-table get_vlc2 used with INIT_VLC_LE tables. */
#ifndef AVCODEC_GET_BITS_H
#define AVCODEC_GET_BITS_H

#include <stdint.h>
#include <string.h>

#include "avcodec.h"
#include "vlc.h"

typedef struct GetBitContext {
	const uint8_t* buffer;
	int index;        /* bit position */
	int size_in_bits; /* total payload bits */
} GetBitContext;

static inline uint32_t ff_shim_rl32(const uint8_t* p)
{
	uint32_t v;
	memcpy(&v, p, 4);
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	v = ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) | ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
#endif
	return v;
}

/* The caller guarantees AV_INPUT_BUFFER_PADDING_SIZE readable bytes past the end,
 * like FFmpeg packets do (the wrapper copies packet data into a padded buffer). */
static inline unsigned show_bits(const GetBitContext* s, int n)
{
	uint32_t cache = ff_shim_rl32(s->buffer + (s->index >> 3)) >> (s->index & 7);
	return cache & ((1u << n) - 1);
}

static inline void skip_bits(GetBitContext* s, int n)
{
	s->index += n;
}

static inline unsigned get_bits(GetBitContext* s, int n)
{
	unsigned value = show_bits(s, n);
	s->index += n;
	return value;
}

static inline unsigned get_bits1(GetBitContext* s)
{
	unsigned value = (s->buffer[s->index >> 3] >> (s->index & 7)) & 1;
	s->index++;
	return value;
}

static inline unsigned get_bits_long(GetBitContext* s, int n)
{
	if (n <= 25) {
		return get_bits(s, n);
	}

	unsigned lo = get_bits(s, 16);
	unsigned hi = get_bits(s, n - 16);
	return lo | (hi << 16);
}

static inline void skip_bits_long(GetBitContext* s, int n)
{
	s->index += n;
}

static inline int get_bits_count(const GetBitContext* s)
{
	return s->index;
}

static inline int get_bits_left(const GetBitContext* s)
{
	return s->size_in_bits - s->index;
}

static inline void align_get_bits(GetBitContext* s)
{
	s->index = (s->index + 7) & ~7;
}

static inline int init_get_bits(GetBitContext* s, const uint8_t* buffer, int bit_size)
{
	if (bit_size < 0 || !buffer) {
		s->buffer = NULL;
		s->index = 0;
		s->size_in_bits = 0;
		return AVERROR_INVALIDDATA;
	}
	s->buffer = buffer;
	s->index = 0;
	s->size_in_bits = bit_size;
	return 0;
}

static inline int init_get_bits8(GetBitContext* s, const uint8_t* buffer, int byte_size)
{
	if (byte_size > INT_MAX / 8 || byte_size < 0) {
		byte_size = -1;
	}
	return init_get_bits(s, buffer, byte_size * 8);
}

/* Two-level table lookup exactly as FFmpeg's GET_VLC macro resolves for LE tables
 * (the table layout is built by the vendored bitstream.c with INIT_VLC_LE). */
static inline int get_vlc2(GetBitContext* s, VLC_TYPE (*table)[2], int bits, int max_depth)
{
	int code = (int) show_bits(s, bits);
	int n = table[code][1];
	code = table[code][0];

	if (n < 0) {
		skip_bits(s, bits);
		int nb_bits = -n;
		code += (int) show_bits(s, nb_bits);
		n = table[code][1];
		code = table[code][0];
		if (n < 0 && max_depth > 2) {
			skip_bits(s, nb_bits);
			nb_bits = -n;
			code += (int) show_bits(s, nb_bits);
			n = table[code][1];
			code = table[code][0];
		}
	}
	skip_bits(s, n);

	return code;
}

#endif /* AVCODEC_GET_BITS_H */
