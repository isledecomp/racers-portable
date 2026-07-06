#ifndef GOLDP_EXPORT_H
#define GOLDP_EXPORT_H

#include "goldp_export_inner.h"

#define GOLDP_EXPORT GOLDP_EXPORT_INNER
#ifdef _WIN32
#define GOLDP_UNIX_EXPORT
#else
#define GOLDP_UNIX_EXPORT GOLDP_EXPORT_INNER
#endif

#endif // GOLDP_EXPORT_H
