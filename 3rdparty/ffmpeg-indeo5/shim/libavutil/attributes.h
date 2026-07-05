/* Shim for FFmpeg's libavutil/attributes.h — just the attributes the vendored
 * Indeo 5 sources use. */
#ifndef AVUTIL_ATTRIBUTES_H
#define AVUTIL_ATTRIBUTES_H

#ifdef __GNUC__
#define av_cold __attribute__((cold))
#define av_always_inline __attribute__((always_inline)) inline
#define av_unused __attribute__((unused))
#define av_const __attribute__((const))
#define av_pure __attribute__((pure))
#else
#define av_cold
#define av_always_inline inline
#define av_unused
#define av_const
#define av_pure
#endif

#endif /* AVUTIL_ATTRIBUTES_H */
