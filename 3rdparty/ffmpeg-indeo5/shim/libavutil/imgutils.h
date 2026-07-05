/* Shim for FFmpeg's libavutil/imgutils.h — dimension sanity check only. */
#ifndef AVUTIL_IMGUTILS_H
#define AVUTIL_IMGUTILS_H

#include <stdint.h>

#include "../avcodec.h"

static inline int av_image_check_size2(
	unsigned int w,
	unsigned int h,
	int64_t max_pixels,
	int pix_fmt,
	int log_offset,
	void* log_ctx
)
{
	(void) pix_fmt;
	(void) log_offset;
	(void) log_ctx;
	if (w == 0 || h == 0 || w > 8192 || h > 8192 || (int64_t) w * h > max_pixels) {
		return AVERROR(22 /* EINVAL */);
	}
	return 0;
}

#endif /* AVUTIL_IMGUTILS_H */
