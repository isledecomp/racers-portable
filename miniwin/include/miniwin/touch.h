#pragma once

// [library:input] Touch input layer: always-additive finger controls on top of the
// DirectInput emulation. Fingers from direct (touchscreen) devices drive an invisible
// analog steering zone with drive-by-touch throttle, drawn race buttons, pause-dialog
// navigation, and menu tap-to-click — while keyboard, mouse and gamepad stay live at
// the same time. The game reads the produced state through a handful of tagged hooks;
// everything else stays portable-side. RACERS_TOUCH=1 turns the desktop mouse into a
// test finger (SDL mouse->touch synthesis) so the whole path can be exercised without
// a touchscreen.

#include <SDL3/SDL.h>

// Race-button bits produced by the touch layer and consumed by the PlayerControls
// hook, in handler order (THROTTLE is the held steering thumb, suppressed while BRAKE
// is down so the brake button brakes instead of the throttle+brake half-thrust).
#define MINIWIN_TOUCH_THROTTLE (1u << 0)
#define MINIWIN_TOUCH_BRAKE (1u << 1)
#define MINIWIN_TOUCH_POWERUP (1u << 2)
#define MINIWIN_TOUCH_DRIFT (1u << 3)
#define MINIWIN_TOUCH_LOOKBACK (1u << 4)

// Synthetic device id used by scripted touch injection (RACERS_AUTOTOUCH): the touch
// layer accepts fingers from it as if they came from a direct touchscreen.
#define MINIWIN_TOUCH_TEST_DEVICE ((SDL_TouchID) 0x52435253) // 'RCRS'

// Player ownership: the first PlayerControls initialized each race session claims the
// touch input (player 0 — the session setup loop initializes players in index order).
// Release ignores non-owners, so the paired Destroy calls are idempotent.
void MiniwinTouch_ClaimPlayer(void* p_owner);
void MiniwinTouch_ReleasePlayer(void* p_owner);

// Analog steering for the owning player: true while a steering finger is down, with
// *p_steer set to the drag deflection in [-1, 1] using the game's analog convention
// (positive steers left, like the negated device axis). Feeds the analog branch of
// PlayerControls::UpdateSteering, so the existing rate shaping applies unchanged.
bool MiniwinTouch_GetSteer(void* p_owner, float* p_steer);

// Race-button edges for the owning player, polled once per PlayerControls::Update.
// p_gate is the game-side "player controlled" condition; while false nothing is
// reported, and when the gate reopens the edge state mirrors from scratch (the game
// reset its input flags while control was away, so still-held buttons re-fire their
// press edges). Returns true when any MINIWIN_TOUCH_* bit changed; the hook applies
// press/release transitions to the matching On* handlers (which are edge-triggered).
bool MiniwinTouch_PollRaceButtons(void* p_owner, bool p_gate, unsigned* p_pressed, unsigned* p_released);

// RaceSession::OpenPauseDialog hook: switches touch to the dialog-navigation cluster
// (up/confirm/down key synthesis) until the race resumes (a gated PollRaceButtons)
// or the session ends (ReleasePlayer). Taps outside the cluster are ignored.
void MiniwinTouch_NotifyRaceDialog();

// MenuTextField focus hook: requests/dismisses the platform on-screen keyboard.
// Deliberately a no-op until a touch device has been used — SDL text input has caused
// macOS window-server stalls, so it is only started where touch actually needs it.
void MiniwinTouch_TextFieldFocus(bool p_focused);

// --- portable application side (Win32GolApp) ---

// Feeds a drained SDL event to the touch layer. Call once per polled event on the
// game thread, alongside MiniwinInput_HandleEvent.
void MiniwinTouch_HandleEvent(const SDL_Event& p_event);

// True while a touch menu-cursor interaction is active (finger down, or lifted within
// a short grace so the release click still dispatches under the finger). While true,
// Win32GolApp::Tick suppresses the real-mouse cursor re-assert and applies
// GetMenuCursor after the event drain instead.
bool MiniwinTouch_MenuCursorActive();

// Latest touch menu-cursor position mapped into the game's logical cursor space
// (p_logicalW/H = GolDrawState dimensions), inverting the present letterbox. Returns
// false when no touch cursor is active.
bool MiniwinTouch_GetMenuCursor(long* p_x, long* p_y, int p_logicalW, int p_logicalH);

// True while a text field is focused on a touch-capable device: Tick then feeds
// SDL_EVENT_TEXT_INPUT to OnChar and suppresses its printable-key synthesis so web
// physical keyboards do not double-type.
bool MiniwinTouch_TextInputActive();
