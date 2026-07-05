// [library:video] DirectShow-based movie playback replaced. The AVI demuxer + Indeo 5
// decoder integration lands with the video/ module; until then the intro movies are
// skipped (equivalent to -novideo).

#include "video/videoplayer.h"

#include "compat.h"
#include "decomp.h"

#include <SDL3/SDL.h>

int VideoPlayer::Begin(Win32GolApp* p_golApp, DWORD p_width, DWORD p_height)
{
	return 1;
}

int VideoPlayer::Play(Win32GolApp* p_golApp, LPCSTR p_filename, int p_abortableOnKey, int p_autoRewind)
{
	SDL_Log("VideoPlayer: skipping '%s' (Indeo 5 playback not implemented yet)", p_filename ? p_filename : "");
	return 1;
}

int VideoPlayer::End(Win32GolApp* p_golApp)
{
	return 1;
}
