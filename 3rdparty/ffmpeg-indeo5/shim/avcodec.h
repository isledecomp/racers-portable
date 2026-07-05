/* Shim for FFmpeg's libavcodec/avcodec.h (plus the libavutil pieces the vendored
 * Indeo 5 sources pull in through it): just enough surface for indeo5.c, ivi.c,
 * ivi_dsp.c and bitstream.c from FFmpeg n4.4.4. */
#ifndef AVCODEC_H
#define AVCODEC_H

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libavutil/attributes.h"
#include "libavutil/avassert.h"
#include "mathops.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- error codes --- */

#define AVERROR(e) (-(e))
#define AVUNERROR(e) (-(e))

#define FFERRTAG(a, b, c, d) (-(int) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned) (d) << 24)))

#define AVERROR_INVALIDDATA FFERRTAG('I', 'N', 'D', 'A')
#define AVERROR_PATCHWELCOME FFERRTAG('P', 'A', 'W', 'E')
#define AVERROR_BUG FFERRTAG('B', 'U', 'G', '!')

/* --- logging --- */

#define AV_LOG_QUIET (-8)
#define AV_LOG_PANIC 0
#define AV_LOG_FATAL 8
#define AV_LOG_ERROR 16
#define AV_LOG_WARNING 24
#define AV_LOG_INFO 32
#define AV_LOG_VERBOSE 40
#define AV_LOG_DEBUG 48

void av_log(void* avcl, int level, const char* fmt, ...);

#define ff_dlog(ctx, ...) ((void) 0)

/* --- memory --- */

void* av_malloc(size_t size);
void* av_mallocz(size_t size);
void* av_malloc_array(size_t nmemb, size_t size);
void* av_mallocz_array(size_t nmemb, size_t size);
void* av_realloc_f(void* ptr, size_t nelem, size_t elsize);
void av_free(void* ptr);
void av_freep(void* ptr);

/* --- common macros / clips --- */

#define FFMIN(a, b) ((a) > (b) ? (b) : (a))
#define FFMAX(a, b) ((a) > (b) ? (a) : (b))
#define FFABS(a) ((a) >= 0 ? (a) : (-(a)))
#define FFSIGN(a) ((a) > 0 ? 1 : -1)
#define FFALIGN(x, a) (((x) + (a) -1) & ~((a) -1))
#define FFSWAP(type, a, b)                                                                                             \
	do {                                                                                                               \
		type SWAP_tmp = b;                                                                                             \
		b = a;                                                                                                         \
		a = SWAP_tmp;                                                                                                  \
	} while (0)

#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))

static av_always_inline av_const int av_clip(int a, int amin, int amax)
{
	if (a < amin) {
		return amin;
	}
	if (a > amax) {
		return amax;
	}
	return a;
}

static av_always_inline av_const uint8_t av_clip_uint8(int a)
{
	if (a & (~0xFF)) {
		return (~a) >> 31;
	}
	return (uint8_t) a;
}

static av_always_inline av_const unsigned av_clip_uintp2(int a, int p)
{
	if (a & ~((1 << p) - 1)) {
		return (unsigned) ((~a) >> 31 & ((1 << p) - 1));
	}
	return (unsigned) a;
}

static av_always_inline uint16_t AV_RB16(const void* p)
{
	const uint8_t* b = (const uint8_t*) p;
	return (uint16_t) ((b[0] << 8) | b[1]);
}

static av_always_inline uint32_t AV_RL32(const void* p)
{
	uint32_t v;
	memcpy(&v, p, 4);
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	v = ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) | ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
#endif
	return v;
}

/* --- enums the decoder touches --- */

enum AVMediaType {
	AVMEDIA_TYPE_VIDEO = 0,
};

enum AVCodecID {
	AV_CODEC_ID_NONE = 0,
	AV_CODEC_ID_INDEO4,
	AV_CODEC_ID_INDEO5,
};

enum AVPixelFormat {
	AV_PIX_FMT_NONE = -1,
	AV_PIX_FMT_YUV410P,
	AV_PIX_FMT_YUVA444P,
};

#define AV_CODEC_CAP_DR1 (1 << 1)

#define AV_INPUT_BUFFER_PADDING_SIZE 64

#define NULL_IF_CONFIG_SMALL(x) x
#define CONFIG_SMALL 0

/* --- core structs --- */

typedef struct AVRational {
	int num;
	int den;
} AVRational;

typedef struct AVPacket {
	uint8_t* data;
	int size;
} AVPacket;

#define AV_NUM_DATA_POINTERS 8

typedef struct AVFrame {
	uint8_t* data[AV_NUM_DATA_POINTERS];
	int linesize[AV_NUM_DATA_POINTERS];
	int width;
	int height;
} AVFrame;

AVFrame* av_frame_alloc(void);
void av_frame_free(AVFrame** frame);
void av_frame_unref(AVFrame* frame);
void av_frame_move_ref(AVFrame* dst, AVFrame* src);

typedef struct AVCodecContext {
	int width;
	int height;
	enum AVPixelFormat pix_fmt;
	enum AVCodecID codec_id;
	void* priv_data;
	int64_t max_pixels;
} AVCodecContext;

typedef struct AVCodec {
	const char* name;
	const char* long_name;
	enum AVMediaType type;
	enum AVCodecID id;
	int priv_data_size;
	int (*init)(AVCodecContext*);
	int (*close)(AVCodecContext*);
	int (*decode)(AVCodecContext*, void* outdata, int* got_frame_ptr, AVPacket* avpkt);
	int capabilities;
	unsigned caps_internal;
} AVCodec;

#ifdef __cplusplus
}
#endif

#endif /* AVCODEC_H */
