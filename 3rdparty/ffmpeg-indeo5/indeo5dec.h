/* Public C API over the vendored FFmpeg Indeo 5 decoder: open a decoder, feed it
 * raw IV50 packets, receive YUV410P frames. */
#ifndef INDEO5DEC_H
#define INDEO5DEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Indeo5Decoder Indeo5Decoder;

typedef struct Indeo5Frame {
	const uint8_t* data[3]; /* Y, U, V planes (4:1:0 subsampling) */
	int linesize[3];
	int width;
	int height;
} Indeo5Frame;

/* Returns NULL on failure. The dimensions are advisory (the bitstream carries its
 * own); they bound the allocation the decoder will accept. */
Indeo5Decoder* Indeo5_Open(int p_maxWidth, int p_maxHeight);

/* Decodes one video packet. Returns 1 and fills p_frame when a picture is ready,
 * 0 when the packet produced no picture, negative on error. The frame planes stay
 * valid until the next Indeo5_Decode or Indeo5_Close call. */
int Indeo5_Decode(Indeo5Decoder* p_decoder, const uint8_t* p_data, size_t p_size, Indeo5Frame* p_frame);

void Indeo5_Close(Indeo5Decoder* p_decoder);

#ifdef __cplusplus
}
#endif

#endif /* INDEO5DEC_H */
