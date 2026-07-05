#pragma once

// [library:filesystem] POSIX shim for the MSVC CRT's <direct.h>, used by the save
// system (directory creation/navigation) and the application shell.

#ifdef __cplusplus
extern "C" {
#endif

int _mkdir(const char* path);
int _rmdir(const char* path);
int _chdir(const char* path);
char* _getcwd(char* buffer, int maxlen);

// MSVC CRT string helper used by the save system.
char* itoa(int value, char* str, int radix);

#ifdef __cplusplus
}
#endif
