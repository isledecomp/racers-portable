// [library:filesystem] Win32 file API + MSVC CRT <io.h> shims over POSIX/SDL, with
// path normalization: the game hard-codes '\\' separators and case-mismatched names
// ("lego.jam" for LEGO.JAM), which only works on Windows filesystems.

#include "handles.h"
#include "miniwin.h"

#include <miniwin/windows.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef __EMSCRIPTEN__
#include <string>
#include <vector>
#endif

#ifdef __EMSCRIPTEN__
// Registry of streamed data files (correct-case absolute mount paths). The web fetch
// backend cannot list a directory, so MiniwinResolvePath case-folds against this instead
// of readdir. A function-local static sidesteps static-init ordering.
static std::vector<std::string>& WebDataFiles()
{
	static std::vector<std::string> files;
	return files;
}

void MiniwinRegisterWebDataFile(const char* p_mountPath)
{
	if (p_mountPath && p_mountPath[0]) {
		WebDataFiles().emplace_back(p_mountPath);
	}
}
#endif

// Resolve a game path to an on-disk path: '\\'→'/', then (on case-sensitive systems)
// resolve each component case-insensitively against the directory listing.
bool MiniwinResolvePath(const char* p_path, char* p_resolved, size_t p_resolvedSize)
{
	if (!p_path || !p_resolved || p_resolvedSize == 0) {
		return false;
	}

	SDL_strlcpy(p_resolved, p_path, p_resolvedSize);
	for (char* c = p_resolved; *c; ++c) {
		if (*c == '\\') {
			*c = '/';
		}
	}

#if defined(__EMSCRIPTEN__)
	// The web fetch backend cannot enumerate a directory, so resolve case from the static
	// registry instead of readdir: match the request's basename against a registered data
	// file (case-insensitive) and return its correct-case mount path. Everything else
	// (absolute OPFS save/preference paths, already-correct paths) passes through.
	{
		const char* base = strrchr(p_resolved, '/');
		base = base ? base + 1 : p_resolved;
		for (const std::string& entry : WebDataFiles()) {
			const char* entryBase = strrchr(entry.c_str(), '/');
			entryBase = entryBase ? entryBase + 1 : entry.c_str();
			if (strcasecmp(entryBase, base) == 0) {
				SDL_strlcpy(p_resolved, entry.c_str(), p_resolvedSize);
				return true;
			}
		}
	}
	return true;
#elif defined(_WIN32)
	return true;
#else
	if (access(p_resolved, F_OK) == 0) {
		return true;
	}

	// Walk the path component by component, fixing case as we go.
	char work[1024];
	SDL_strlcpy(work, p_resolved, sizeof(work));

	char out[1024];
	const char* cursor = work;
	if (work[0] == '/') {
		out[0] = '/';
		out[1] = '\0';
		cursor = work + 1;
	}
	else {
		out[0] = '\0';
	}

	while (*cursor) {
		char component[512];
		size_t len = 0;
		while (cursor[len] && cursor[len] != '/') {
			len++;
		}
		if (len >= sizeof(component)) {
			return false;
		}
		SDL_memcpy(component, cursor, len);
		component[len] = '\0';
		cursor += len;
		while (*cursor == '/') {
			cursor++;
		}

		char candidate[1024];
		SDL_snprintf(
			candidate,
			sizeof(candidate),
			"%s%s%s",
			out,
			out[0] && out[SDL_strlen(out) - 1] != '/' ? "/" : "",
			component
		);

		if (access(candidate, F_OK) != 0) {
			// Case-insensitive lookup in the parent directory.
			const char* parent = out[0] ? out : ".";
			DIR* dir = opendir(parent);
			bool found = false;
			if (dir) {
				struct dirent* entry;
				while ((entry = readdir(dir)) != nullptr) {
					if (strcasecmp(entry->d_name, component) == 0) {
						SDL_snprintf(
							candidate,
							sizeof(candidate),
							"%s%s%s",
							out,
							out[0] && out[SDL_strlen(out) - 1] != '/' ? "/" : "",
							entry->d_name
						);
						found = true;
						break;
					}
				}
				closedir(dir);
			}
			if (!found) {
				// Leave the normalized (unresolved) remainder for creation use-cases.
				SDL_strlcpy(p_resolved, candidate, p_resolvedSize);
				if (*cursor) {
					SDL_strlcat(p_resolved, "/", p_resolvedSize);
					SDL_strlcat(p_resolved, cursor, p_resolvedSize);
				}
				return false;
			}
		}

		SDL_strlcpy(out, candidate, sizeof(out));
	}

	SDL_strlcpy(p_resolved, out, p_resolvedSize);
	return true;
#endif
}

void MiniwinGetUserDataPath(char* p_out, size_t p_size)
{
	if (!p_out || p_size == 0) {
		return;
	}

#ifdef __EMSCRIPTEN__
	// SDL_GetPrefPath is a non-persistent MEMFS path on the web; use the OPFS mount set up
	// by Racers_SetupWebFilesystem (LEGORacers/emscripten/filesystem.cpp) so saves and the
	// renderer preference survive reloads. The trailing slash matches SDL_GetPrefPath.
	// Keep in sync with Racers_userDataMount ("/racers-user").
	SDL_strlcpy(p_out, "/racers-user/", p_size);
#else
	// SDL_GetPrefPath creates the directory and is comparatively costly, so cache it. The
	// returned path already ends with the platform path separator.
	static char s_base[1024];
	if (!s_base[0]) {
		char* pref = SDL_GetPrefPath("isledecomp", "racers");
		if (pref) {
			SDL_strlcpy(s_base, pref, sizeof(s_base));
			SDL_free(pref);
		}
	}

	SDL_strlcpy(p_out, s_base, p_size);
#endif
}

// --- Win32 file API (used by the music streaming code) ---

HANDLE CreateFile(
	LPCSTR lpFileName,
	DWORD dwDesiredAccess,
	DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes,
	HANDLE hTemplateFile
)
{
	MiniwinPhaseScope phase(MINIWIN_PHASE_FILE_IO);
	MiniwinSlowOpLog slowLog("CreateFile", lpFileName);

	char resolved[1024];
	MiniwinResolvePath(lpFileName, resolved, sizeof(resolved));

	const char* mode;
	if (dwDesiredAccess & GENERIC_WRITE) {
		mode = (dwCreationDisposition == CREATE_ALWAYS) ? "wb" : "r+b";
	}
	else {
		mode = "rb";
	}

	SDL_RWops* stream = SDL_RWFromFile(resolved, mode);
	if (!stream) {
		return INVALID_HANDLE_VALUE;
	}

	return new MiniwinFileHandle(stream);
}

BOOL ReadFile(
	HANDLE hFile,
	LPVOID lpBuffer,
	DWORD nNumberOfBytesToRead,
	LPDWORD lpNumberOfBytesRead,
	LPOVERLAPPED lpOverlapped
)
{
	MiniwinHandle* handle = static_cast<MiniwinHandle*>(hFile);
	if (!handle || handle == INVALID_HANDLE_VALUE || handle->type != MiniwinHandleType::File) {
		return FALSE;
	}

	MiniwinPhaseScope phase(MINIWIN_PHASE_FILE_IO);
	MiniwinSlowOpLog slowLog("ReadFile", "");

	size_t read = SDL_RWread(static_cast<MiniwinFileHandle*>(handle)->stream, lpBuffer, 1, nNumberOfBytesToRead);
	if (lpNumberOfBytesRead) {
		*lpNumberOfBytesRead = (DWORD) read;
	}

	return TRUE;
}

BOOL WriteFile(
	HANDLE hFile,
	LPCVOID lpBuffer,
	DWORD nNumberOfBytesToWrite,
	LPDWORD lpNumberOfBytesWritten,
	LPOVERLAPPED lpOverlapped
)
{
	MiniwinHandle* handle = static_cast<MiniwinHandle*>(hFile);
	if (!handle || handle == INVALID_HANDLE_VALUE || handle->type != MiniwinHandleType::File) {
		return FALSE;
	}

	size_t written = SDL_RWwrite(static_cast<MiniwinFileHandle*>(handle)->stream, lpBuffer, 1, nNumberOfBytesToWrite);
	if (lpNumberOfBytesWritten) {
		*lpNumberOfBytesWritten = (DWORD) written;
	}

	return written == nNumberOfBytesToWrite;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LPLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
	MiniwinHandle* handle = static_cast<MiniwinHandle*>(hFile);
	if (!handle || handle == INVALID_HANDLE_VALUE || handle->type != MiniwinHandleType::File) {
		return INVALID_SET_FILE_POINTER;
	}

	int whence;
	switch (dwMoveMethod) {
	case FILE_BEGIN:
		whence = RW_SEEK_SET;
		break;
	case FILE_CURRENT:
		whence = RW_SEEK_CUR;
		break;
	case FILE_END:
		whence = RW_SEEK_END;
		break;
	default:
		return INVALID_SET_FILE_POINTER;
	}

	Sint64 pos = SDL_RWseek(static_cast<MiniwinFileHandle*>(handle)->stream, lDistanceToMove, whence);
	if (pos < 0) {
		return INVALID_SET_FILE_POINTER;
	}

	return (DWORD) pos;
}

DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh)
{
	MiniwinHandle* handle = static_cast<MiniwinHandle*>(hFile);
	if (!handle || handle == INVALID_HANDLE_VALUE || handle->type != MiniwinHandleType::File) {
		return INVALID_FILE_SIZE;
	}

	Sint64 size = SDL_RWsize(static_cast<MiniwinFileHandle*>(handle)->stream);
	if (size < 0) {
		return INVALID_FILE_SIZE;
	}

	if (lpFileSizeHigh) {
		*lpFileSizeHigh = (DWORD) ((Uint64) size >> 32);
	}

	return (DWORD) size;
}

DWORD GetFileAttributes(LPCSTR lpFileName)
{
	char resolved[1024];
	if (!MiniwinResolvePath(lpFileName, resolved, sizeof(resolved))) {
		return INVALID_FILE_ATTRIBUTES;
	}

	return FILE_ATTRIBUTE_NORMAL;
}

// --- MSVC CRT <io.h> shims (used by common/src/golfile.cpp) ---

#ifndef _WIN32

extern "C"
{

	int _open(const char* path, int oflag, int pmode)
	{
		MiniwinPhaseScope phase(MINIWIN_PHASE_FILE_IO);
		MiniwinSlowOpLog slowLog("_open", path);

		char resolved[1024];
		MiniwinResolvePath(path, resolved, sizeof(resolved));
		int fd = open(resolved, oflag, (mode_t) pmode);

		static const char* fsLog = getenv("RACERS_FS_LOG");
		if (fsLog) {
			SDL_LogInfo(LOG_CATEGORY_MINIWIN, "_open('%s' -> '%s') = %d", path, resolved, fd);
		}

		return fd;
	}

	int _close(int fd)
	{
		return close(fd);
	}

	int _read(int fd, void* buffer, unsigned int count)
	{
		MiniwinPhaseScope phase(MINIWIN_PHASE_FILE_IO);
		MiniwinSlowOpLog slowLog("_read", "");

		int result = (int) read(fd, buffer, count);

		static const char* fsLog = getenv("RACERS_FS_LOG");
		static int logged = 0;
		if (fsLog && logged < 40) {
			logged++;
			SDL_LogInfo(LOG_CATEGORY_MINIWIN, "_read(fd=%d, %u) = %d", fd, count, result);
		}

		return result;
	}

	int _write(int fd, const void* buffer, unsigned int count)
	{
		return (int) write(fd, buffer, count);
	}

	long _lseek(int fd, long offset, int origin)
	{
		return (long) lseek(fd, offset, origin);
	}

	long _tell(int fd)
	{
		return (long) lseek(fd, 0, SEEK_CUR);
	}

	int _access(const char* path, int mode)
	{
		char resolved[1024];
		MiniwinResolvePath(path, resolved, sizeof(resolved));
		return access(resolved, mode == 0 ? F_OK : mode);
	}

	int _commit(int fd)
	{
		return fsync(fd);
	}

	int _mkdir(const char* path)
	{
		char resolved[1024];
		MiniwinResolvePath(path, resolved, sizeof(resolved));
		return mkdir(resolved, 0755);
	}

	int _rmdir(const char* path)
	{
		char resolved[1024];
		MiniwinResolvePath(path, resolved, sizeof(resolved));
		return rmdir(resolved);
	}

	int _chdir(const char* path)
	{
		char resolved[1024];
		MiniwinResolvePath(path, resolved, sizeof(resolved));
		return chdir(resolved);
	}

	char* _getcwd(char* buffer, int maxlen)
	{
		return getcwd(buffer, (size_t) maxlen);
	}

	char* itoa(int value, char* str, int radix)
	{
		if (radix == 16) {
			SDL_snprintf(str, 33, "%x", value);
		}
		else {
			SDL_snprintf(str, 33, "%d", value);
		}
		return str;
	}

} // extern "C"

#endif // !_WIN32
