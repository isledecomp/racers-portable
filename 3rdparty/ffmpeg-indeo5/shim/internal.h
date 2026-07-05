/* Shim for FFmpeg's libavcodec/internal.h. */
#ifndef AVCODEC_INTERNAL_H
#define AVCODEC_INTERNAL_H

#include "avcodec.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FF_CODEC_CAP_INIT_THREADSAFE (1 << 0)
#define FF_CODEC_CAP_INIT_CLEANUP (1 << 1)

int ff_get_buffer(AVCodecContext* avctx, AVFrame* frame, int flags);
int ff_set_dimensions(AVCodecContext* avctx, int width, int height);

void avpriv_report_missing_feature(void* avc, const char* msg, ...);
void avpriv_request_sample(void* avc, const char* msg, ...);

#ifdef __cplusplus
}
#endif

#endif /* AVCODEC_INTERNAL_H */
