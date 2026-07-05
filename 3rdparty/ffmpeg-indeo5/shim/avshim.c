/* Support implementation for the FFmpeg shim headers: logging, memory, frame
 * management and the lookup tables the vendored Indeo 5 sources reference. */
#include "avcodec.h"
#include "internal.h"

#include <stdio.h>

/* --- logging --- */

void av_log(void* avcl, int level, const char* fmt, ...)
{
	(void) avcl;
	if (level > AV_LOG_WARNING) {
		return;
	}

	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "[indeo5] ");
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void avpriv_report_missing_feature(void* avc, const char* msg, ...)
{
	(void) avc;
	va_list args;
	va_start(args, msg);
	fprintf(stderr, "[indeo5] missing feature: ");
	vfprintf(stderr, msg, args);
	fprintf(stderr, "\n");
	va_end(args);
}

void avpriv_request_sample(void* avc, const char* msg, ...)
{
	(void) avc;
	va_list args;
	va_start(args, msg);
	fprintf(stderr, "[indeo5] unsupported sample: ");
	vfprintf(stderr, msg, args);
	fprintf(stderr, "\n");
	va_end(args);
}

/* --- memory --- */

void* av_malloc(size_t size)
{
	return malloc(size ? size : 1);
}

void* av_mallocz(size_t size)
{
	return calloc(1, size ? size : 1);
}

void* av_malloc_array(size_t nmemb, size_t size)
{
	if (size && nmemb > SIZE_MAX / size) {
		return NULL;
	}
	return av_malloc(nmemb * size);
}

void* av_mallocz_array(size_t nmemb, size_t size)
{
	if (size && nmemb > SIZE_MAX / size) {
		return NULL;
	}
	return av_mallocz(nmemb * size);
}

void* av_realloc_f(void* ptr, size_t nelem, size_t elsize)
{
	if (elsize && nelem > SIZE_MAX / elsize) {
		free(ptr);
		return NULL;
	}

	void* result = realloc(ptr, nelem * elsize);
	if (!result && nelem && elsize) {
		free(ptr);
	}
	return result;
}

void av_free(void* ptr)
{
	free(ptr);
}

void av_freep(void* ptr)
{
	void** p = (void**) ptr;
	free(*p);
	*p = NULL;
}

/* --- frames --- */

AVFrame* av_frame_alloc(void)
{
	return (AVFrame*) calloc(1, sizeof(AVFrame));
}

void av_frame_unref(AVFrame* frame)
{
	if (!frame) {
		return;
	}
	/* data[0] owns the single allocation made by ff_get_buffer. */
	free(frame->data[0]);
	memset(frame, 0, sizeof(*frame));
}

void av_frame_free(AVFrame** frame)
{
	if (!frame || !*frame) {
		return;
	}
	av_frame_unref(*frame);
	free(*frame);
	*frame = NULL;
}

void av_frame_move_ref(AVFrame* dst, AVFrame* src)
{
	*dst = *src;
	memset(src, 0, sizeof(*src));
}

/* --- decode support --- */

int ff_set_dimensions(AVCodecContext* avctx, int width, int height)
{
	if (width <= 0 || height <= 0 || (int64_t) width * height > avctx->max_pixels) {
		return AVERROR(EINVAL);
	}
	avctx->width = width;
	avctx->height = height;
	return 0;
}

/* YUV410P only: one allocation holding the three planes. */
int ff_get_buffer(AVCodecContext* avctx, AVFrame* frame, int flags)
{
	(void) flags;
	if (avctx->pix_fmt != AV_PIX_FMT_YUV410P || avctx->width <= 0 || avctx->height <= 0) {
		return AVERROR(EINVAL);
	}

	int lumaStride = FFALIGN(avctx->width, 32);
	int lumaHeight = FFALIGN(avctx->height, 4);
	int chromaStride = FFALIGN((avctx->width + 3) / 4, 32);
	int chromaHeight = (lumaHeight + 3) / 4;

	size_t lumaSize = (size_t) lumaStride * lumaHeight;
	size_t chromaSize = (size_t) chromaStride * chromaHeight;
	uint8_t* memory = (uint8_t*) calloc(1, lumaSize + 2 * chromaSize + 64);
	if (!memory) {
		return AVERROR(ENOMEM);
	}

	frame->data[0] = memory;
	frame->data[1] = memory + lumaSize;
	frame->data[2] = memory + lumaSize + chromaSize;
	frame->linesize[0] = lumaStride;
	frame->linesize[1] = chromaStride;
	frame->linesize[2] = chromaStride;
	frame->width = avctx->width;
	frame->height = avctx->height;
	return 0;
}

/* --- tables --- */

const uint8_t ff_zigzag_direct[64] = {
	 0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25,
	18, 11,  4,  5, 12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28, 35, 42, 49, 56,
	57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54,
	47, 55, 62, 63
};

const uint8_t ff_reverse[256] = {
	0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0,
	0x30, 0xB0, 0x70, 0xF0, 0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8,
	0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 0x04, 0x84, 0x44, 0xC4,
	0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
	0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC,
	0x3C, 0xBC, 0x7C, 0xFC, 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2,
	0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 0x0A, 0x8A, 0x4A, 0xCA,
	0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
	0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6,
	0x36, 0xB6, 0x76, 0xF6, 0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
	0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, 0x01, 0x81, 0x41, 0xC1,
	0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
	0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9,
	0x39, 0xB9, 0x79, 0xF9, 0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5,
	0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5, 0x0D, 0x8D, 0x4D, 0xCD,
	0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
	0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3,
	0x33, 0xB3, 0x73, 0xF3, 0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB,
	0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB, 0x07, 0x87, 0x47, 0xC7,
	0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
	0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF,
	0x3F, 0xBF, 0x7F, 0xFF
};
