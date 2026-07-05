/* Shim for FFmpeg's libavutil/avassert.h. */
#ifndef AVUTIL_AVASSERT_H
#define AVUTIL_AVASSERT_H

#include <assert.h>

#define av_assert0(cond) assert(cond)
#define av_assert1(cond) assert(cond)
#define av_assert2(cond) ((void) 0)

#endif /* AVUTIL_AVASSERT_H */
