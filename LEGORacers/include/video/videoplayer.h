#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

// [library:video] The DirectShow-based player was replaced; playback of the original
// Indeo 5 AVIs is provided by the in-tree video/ module (AVI demuxer + Indeo 5
// decoder) presented through SDL3. The public interface used by LegoRacers::Run() is
// unchanged.

#include "app/win32golapp.h"
#include "types.h"

#include <windows.h>

class VideoPlayer {
public:
	static int Begin(Win32GolApp* p_golApp, DWORD p_width, DWORD p_height);
	static int Play(Win32GolApp* p_golApp, LPCSTR p_filename, int p_abortableOnKey, int p_autoRewind);
	static int End(Win32GolApp* p_golApp);
};

#endif // VIDEOPLAYER_H
