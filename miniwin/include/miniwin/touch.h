#pragma once

// Touch layer stubs — touch is out of scope for the SDL2 port.
// These functions are referenced by the decompiled game code but always return
// defaults appropriate for keyboard/mouse-only input.

#include <miniwin/windows.h>

class GolDirectInputDevice;

inline void MiniwinTouch_TextFieldFocus(bool) {}
inline void MiniwinTouch_ReleasePlayer(void*) {}
inline void MiniwinTouch_ClaimPlayer(void*) {}
inline void MiniwinTouch_SetLocalPlayerCount(int) {}
inline void MiniwinTouch_NotifyRaceDialog() {}
inline bool MiniwinTouch_GetSteer(void*, float*) { return false; }
inline bool MiniwinTouch_PollRaceButtons(void*, bool, unsigned*, unsigned*) { return false; }
inline bool MiniwinTouch_MenuCursorActive() { return false; }
inline bool MiniwinTouch_TextInputActive() { return false; }
inline bool MiniwinTouch_GetMenuCursor(long*, long*, int, int) { return false; }
