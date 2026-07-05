#pragma once

// [library:filesystem] POSIX shim for the MSVC CRT's <io.h>, used by common/src/golfile.cpp.
// Paths coming from the game may contain backslash separators and case-mismatched names
// (e.g. "lego.jam" for LEGO.JAM); the implementation normalizes and resolves them.

#include <fcntl.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _O_RDONLY
#define _O_RDONLY O_RDONLY
#define _O_WRONLY O_WRONLY
#define _O_RDWR O_RDWR
#define _O_APPEND O_APPEND
#define _O_CREAT O_CREAT
#define _O_TRUNC O_TRUNC
#define _O_EXCL O_EXCL
#endif

#ifndef _O_BINARY
#define _O_BINARY 0
#define _O_TEXT 0
#endif

#ifndef _S_IREAD
#define _S_IREAD S_IRUSR
#define _S_IWRITE S_IWUSR
#endif

int _open(const char* path, int oflag, int pmode);
int _close(int fd);
int _read(int fd, void* buffer, unsigned int count);
int _write(int fd, const void* buffer, unsigned int count);
long _lseek(int fd, long offset, int origin);
long _tell(int fd);
int _access(const char* path, int mode);
int _commit(int fd);

#ifdef __cplusplus
}
#endif
