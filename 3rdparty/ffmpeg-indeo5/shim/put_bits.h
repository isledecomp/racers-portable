/* Shim for FFmpeg's libavcodec/put_bits.h — bitstream.c's ff_put_string needs a
 * working writer even though the Indeo decode path never calls it. */
#ifndef AVCODEC_PUT_BITS_H
#define AVCODEC_PUT_BITS_H

#include <stdint.h>

#define FF_API_AVPRIV_PUT_BITS 0

typedef struct PutBitContext {
	uint64_t bit_buf;
	int bit_left;
	uint8_t* buf;
	uint8_t* buf_ptr;
	uint8_t* buf_end;
} PutBitContext;

static inline void put_bits(PutBitContext* s, int n, unsigned int value)
{
	s->bit_buf = (s->bit_buf << n) | value;
	s->bit_left -= n;
	while (s->bit_left <= 56 && s->buf_ptr < s->buf_end) {
		*s->buf_ptr++ = (uint8_t) (s->bit_buf >> (56 - s->bit_left));
		s->bit_left += 8;
	}
}

static inline int put_bits_left(PutBitContext* s)
{
	return (int) (s->buf_end - s->buf_ptr) * 8 + s->bit_left - 64;
}

static inline int put_bits_count(PutBitContext* s)
{
	return (int) (s->buf_ptr - s->buf) * 8 + 64 - s->bit_left;
}

static inline void flush_put_bits(PutBitContext* s)
{
	while (s->bit_left < 64 && s->buf_ptr < s->buf_end) {
		*s->buf_ptr++ = (uint8_t) (s->bit_buf >> (56 - (64 - s->bit_left - 8)));
		s->bit_left += 8;
	}
	s->bit_left = 64;
	s->bit_buf = 0;
}

static inline uint8_t* put_bits_ptr(PutBitContext* s)
{
	return s->buf_ptr;
}

static inline void skip_put_bytes(PutBitContext* s, int n)
{
	s->buf_ptr += n;
}

#endif /* AVCODEC_PUT_BITS_H */
