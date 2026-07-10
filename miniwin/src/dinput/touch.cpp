// [library:input] Touch input layer implementation. Everything runs on the game
// thread: Win32GolApp::Tick drains SDL finger events into MiniwinTouch_HandleEvent,
// the PlayerControls/RaceSession hooks poll the produced state during the game
// update, and the overlay reads its draw list at present time — so no locking is
// needed (the web build is single-threaded through the same path).
//
// Mode model: a finger's role is decided once at finger-down from the current mode
// and stays sticky until finger-up/cancel, so a mode flip mid-hold (pausing while
// steering) never reinterprets a held finger.
//   RaceDialog — RaceSession opened the pause/end dialog: fingers press the
//                up/confirm/down cluster (key synthesis); a miss is ignored (the
//                dialog owns the screen). Quitting to the menus clears the cluster
//                through the session teardown's ReleasePlayer.
//   Race       — a PlayerControls owner is claimed (a race session is live): fingers
//                steer (left half) or press race buttons. Ownership, not the control
//                gate, keeps the overlay and finger roles stable while the game
//                briefly takes control away (countdown, crash tumble, respawn);
//                button edges freeze while the gate is closed and deliver when
//                control returns.
//   Menu       — otherwise: the first finger becomes the menu cursor (tap-to-click
//                through the emulated mouse button + cursor override in Tick).
//   Disabled   — an owner is claimed but the session has more than one local player
//                (Versus split screen): touch could only ever drive player one, so
//                every finger is ignored until the session ends — no steering, no
//                overlay, no dialog cluster, and no menu-cursor clicks into the
//                exclusive-mode race input.

#include "renderbackend.h"
#include "touchoverlay.h"

#include <miniwin/miniwinapp.h>
#include <miniwin/touch.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// --- Layout tunables (fractions of min(drawableW, drawableH) unless noted) ---

static const float g_steerRadiusFrac = 0.12f; // drag distance for full steering lock
static const float g_brakeSizeFrac = 0.16f;   // BRAKE, bottom-right anchor
static const float g_primarySizeFrac = 0.14f; // POWER-UP / SLIDE
static const float g_clusterGapFrac = 0.035f;
static const float g_clusterMarginFrac = 0.04f;
static const float g_cornerSizeFrac = 0.10f; // PAUSE / LOOK-BACK
static const float g_cornerMarginFrac = 0.025f;
static const float g_clusterCenterFrac = 0.46f; // right-edge action arc vertical center
static const float g_dialogSizeFrac = 0.14f;    // pause-dialog up / confirm / down
static const Uint64 g_menuCursorGraceMs = 120;  // cursor override linger after finger-up

// --- State ---

enum TouchButton {
	c_touchButtonNone = -1,
	c_touchButtonPause = 0,
	c_touchButtonLookBack,
	c_touchButtonPowerup,
	c_touchButtonSlide,
	c_touchButtonBrake,
	c_touchButtonDialogUp,
	c_touchButtonDialogConfirm,
	c_touchButtonDialogDown,
	c_touchButtonCount,
};

enum class TouchRole {
	Ignored,
	Steer,
	Button,
	DialogButton,
	MenuCursor,
};

enum class TouchMode {
	Menu,
	Race,
	RaceDialog,
	Disabled,
};

struct TouchFinger {
	SDL_TouchID m_touchId;
	SDL_FingerID m_fingerId;
	TouchRole m_role;
	TouchButton m_button; // Button / DialogButton roles
	float m_originNX;     // finger-down position (normalized; the Steer origin)
	float m_originNY;
	float m_nx; // latest position (normalized window coordinates)
	float m_ny;
	bool m_fromMouse; // synthesized by RACERS_TOUCH mouse debugging
};

static TouchFinger g_fingers[10];
static int g_fingerCount = 0;

static void* g_playerOwner = nullptr;
static bool g_multiPlayerSession = false;
static unsigned g_prevButtons = 0;
static bool g_raceGateOpen = false;
static bool g_dialogPending = false;
static bool g_textFieldFocused = false;
static bool g_everTouched = false;

static SDL_Window* g_window = nullptr; // window of the last accepted finger event
static int g_drawableW = 0;
static int g_drawableH = 0;

static bool g_menuCursorDown = false;
static Uint64 g_menuCursorUpAtMs = 0; // grace window start after release
static float g_menuCursorNX = 0.0f;
static float g_menuCursorNY = 0.0f;

static bool TouchDebugEnabled()
{
	static int enabled = -1;
	if (enabled < 0) {
		const char* env = SDL_getenv("RACERS_TOUCH");
		enabled = env && SDL_atoi(env) ? 1 : 0;
	}
	return enabled == 1;
}

static TouchMode CurrentMode()
{
	// Stand-down first: Versus sessions still claim ownership (the claim brackets the
	// session and its release clears the dialog flag), but no finger may act on it.
	if (g_playerOwner && g_multiPlayerSession) {
		return TouchMode::Disabled;
	}
	// Dialog next: the control gate is closed while a dialog is open, but the
	// session (and thus the owner claim) stays alive.
	if (g_dialogPending) {
		return TouchMode::RaceDialog;
	}
	if (g_playerOwner) {
		return TouchMode::Race;
	}
	return TouchMode::Menu;
}

// --- Layout & hit-testing (drawable pixels) ---

struct TouchRect {
	float m_x;
	float m_y;
	float m_w;
	float m_h;
};

struct TouchLayout {
	TouchRect m_buttons[c_touchButtonCount];
	float m_steerRadiusPx;
};

static void ComputeLayout(TouchLayout& p_layout)
{
	float dw = (float) g_drawableW;
	float dh = (float) g_drawableH;
	float unit = dw < dh ? dw : dh;

	// Safe-area insets arrive in window points; scale them into drawable pixels.
	float insetL = 0.0f;
	float insetT = 0.0f;
	float insetR = 0.0f;
	float insetB = 0.0f;
	if (g_window) {
		SDL_Rect safe;
		int windowW = 0;
		int windowH = 0;
		if (SDL_GetWindowSafeArea(g_window, &safe) && SDL_GetWindowSize(g_window, &windowW, &windowH) && windowW > 0 &&
			windowH > 0) {
			float scaleX = dw / (float) windowW;
			float scaleY = dh / (float) windowH;
			insetL = (float) safe.x * scaleX;
			insetT = (float) safe.y * scaleY;
			insetR = (float) (windowW - (safe.x + safe.w)) * scaleX;
			insetB = (float) (windowH - (safe.y + safe.h)) * scaleY;
		}
	}

	float corner = g_cornerSizeFrac * unit;
	float cornerMargin = g_cornerMarginFrac * unit;
	float brake = g_brakeSizeFrac * unit;
	float primary = g_primarySizeFrac * unit;
	float gap = g_clusterGapFrac * unit;
	float margin = g_clusterMarginFrac * unit;
	float dialog = g_dialogSizeFrac * unit;

	p_layout.m_steerRadiusPx = g_steerRadiusFrac * unit;

	// The game HUD owns three corners — race position (top-left), lap/time (top-right)
	// and the minimap (bottom-right) — so the touch controls stay clear of them: the
	// two small buttons sit in the free top-center strip, and the right-thumb action
	// cluster is a vertical arc centered on the right edge, in the gap between the top
	// timer and the bottom minimap.
	float centerX = dw * 0.5f;
	float topY = insetT + cornerMargin;
	p_layout.m_buttons[c_touchButtonPause] = {centerX - corner - gap * 0.5f, topY, corner, corner};
	p_layout.m_buttons[c_touchButtonLookBack] = {centerX + gap * 0.5f, topY, corner, corner};

	// Right-thumb action arc, centered vertically on the right edge: BRAKE (largest)
	// hugs the edge, POWER-UP sits up-and-in and SLIDE down-and-in, so the thumb sweeps
	// the arc. g_clusterCenterFrac is nudged above mid-height so the lowest button
	// clears the minimap.
	float clusterCX = dw - insetR - margin - brake * 0.5f;
	float clusterCY = dh * g_clusterCenterFrac;
	float innerDX = brake * 0.35f + primary * 0.5f;
	p_layout.m_buttons[c_touchButtonBrake] = {clusterCX - brake * 0.5f, clusterCY - brake * 0.5f, brake, brake};
	p_layout.m_buttons[c_touchButtonPowerup] =
		{clusterCX - innerDX, clusterCY - brake * 0.5f - gap - primary, primary, primary};
	p_layout.m_buttons[c_touchButtonSlide] = {clusterCX - innerDX, clusterCY + brake * 0.5f + gap, primary, primary};

	// Pause-dialog navigation column, right edge, vertically centered (the HUD is
	// hidden while the dialog is open, so this shares the action cluster's home).
	float dialogX = dw - insetR - margin - dialog;
	float dialogY = dh * 0.5f - dialog * 1.5f - gap;
	p_layout.m_buttons[c_touchButtonDialogUp] = {dialogX, dialogY, dialog, dialog};
	p_layout.m_buttons[c_touchButtonDialogConfirm] = {dialogX, dialogY + dialog + gap, dialog, dialog};
	p_layout.m_buttons[c_touchButtonDialogDown] = {dialogX, dialogY + (dialog + gap) * 2.0f, dialog, dialog};
}

static bool HitTest(const TouchRect& p_rect, float p_x, float p_y)
{
	return p_x >= p_rect.m_x && p_x < p_rect.m_x + p_rect.m_w && p_y >= p_rect.m_y && p_y < p_rect.m_y + p_rect.m_h;
}

static TouchButton HitTestButtons(
	const TouchLayout& p_layout,
	float p_x,
	float p_y,
	TouchButton p_first,
	TouchButton p_last
)
{
	for (int i = p_first; i <= p_last; i++) {
		if (HitTest(p_layout.m_buttons[i], p_x, p_y)) {
			return (TouchButton) i;
		}
	}
	return c_touchButtonNone;
}

// --- Key synthesis (PumpAutoKeys pattern: rides the normal event queue, which is
// drained by the same loop that delivered the finger event, so it lands this frame) ---

static void PushKeyEvent(SDL_Scancode p_scancode, bool p_down)
{
	SDL_Event event;
	SDL_zero(event);
	event.type = p_down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
	event.key.scancode = p_scancode;
	event.key.key = SDL_GetKeyFromScancode(p_scancode, 0, false);
	event.key.down = p_down;
	event.key.repeat = false;
	event.key.mod = SDL_KMOD_NONE;
	MiniwinApp_PushEvent(event);
}

static SDL_Scancode ButtonScancode(TouchButton p_button)
{
	switch (p_button) {
	case c_touchButtonPause:
		return SDL_SCANCODE_ESCAPE;
	case c_touchButtonDialogUp:
		return SDL_SCANCODE_UP;
	case c_touchButtonDialogConfirm:
		return SDL_SCANCODE_RETURN;
	case c_touchButtonDialogDown:
		return SDL_SCANCODE_DOWN;
	default:
		return SDL_SCANCODE_UNKNOWN;
	}
}

// --- Finger bookkeeping ---

static TouchFinger* FindFinger(SDL_TouchID p_touchId, SDL_FingerID p_fingerId)
{
	for (int i = 0; i < g_fingerCount; i++) {
		if (g_fingers[i].m_touchId == p_touchId && g_fingers[i].m_fingerId == p_fingerId) {
			return &g_fingers[i];
		}
	}
	return nullptr;
}

static bool RoleActive(TouchRole p_role)
{
	for (int i = 0; i < g_fingerCount; i++) {
		if (g_fingers[i].m_role == p_role) {
			return true;
		}
	}
	return false;
}

static unsigned ComputeButtons()
{
	bool steer = false;
	bool brake = false;
	unsigned bits = 0;
	for (int i = 0; i < g_fingerCount; i++) {
		const TouchFinger& finger = g_fingers[i];
		if (finger.m_role == TouchRole::Steer) {
			steer = true;
		}
		else if (finger.m_role == TouchRole::Button) {
			switch (finger.m_button) {
			case c_touchButtonBrake:
				bits |= MINIWIN_TOUCH_BRAKE;
				brake = true;
				break;
			case c_touchButtonPowerup:
				bits |= MINIWIN_TOUCH_POWERUP;
				break;
			case c_touchButtonSlide:
				bits |= MINIWIN_TOUCH_DRIFT;
				break;
			case c_touchButtonLookBack:
				bits |= MINIWIN_TOUCH_LOOKBACK;
				break;
			default:
				break;
			}
		}
	}
	// Drive-by-touch: the held steering thumb throttles; an explicit BRAKE press wins
	// so braking is real braking rather than the game's throttle+brake half-thrust.
	if (steer && !brake) {
		bits |= MINIWIN_TOUCH_THROTTLE;
	}
	return bits;
}

static void BecomeMenuCursor(TouchFinger& p_finger)
{
	if (p_finger.m_fromMouse || RoleActive(TouchRole::MenuCursor)) {
		// The real mouse already moves and clicks the menus (RACERS_TOUCH debugging
		// would double-click); extra fingers don't steal the cursor mid-interaction.
		return;
	}
	p_finger.m_role = TouchRole::MenuCursor;
	g_menuCursorDown = true;
	g_menuCursorUpAtMs = 0;
	g_menuCursorNX = p_finger.m_nx;
	g_menuCursorNY = p_finger.m_ny;
	// The click itself is injected at finger-up: the menu only runs its hover pass
	// (widget focus following the cursor) on frames with mouse motion, and a widget
	// must be hovered/focused before a click lands on it. The nudge forces that
	// hover pass at the asserted cursor position; windowed mode discards the delta
	// itself (the absolute cursor path owns the position).
	MiniwinInput_InjectMouseMotion(1, 0);
}

static void HandleFingerDown(const SDL_TouchFingerEvent& p_finger)
{
	bool fromMouse = p_finger.touchID == SDL_MOUSE_TOUCHID;
	bool direct = !fromMouse && (p_finger.touchID == MINIWIN_TOUCH_TEST_DEVICE ||
								 SDL_GetTouchDeviceType(p_finger.touchID) == SDL_TOUCH_DEVICE_DIRECT);
	if (!direct && !(fromMouse && TouchDebugEnabled())) {
		// Trackpads and other indirect devices must not steer or click.
		return;
	}
	g_everTouched = true;

	SDL_Window* window = SDL_GetWindowFromID(p_finger.windowID);
	if (window) {
		g_window = window;
		SDL_GetWindowSizeInPixels(window, &g_drawableW, &g_drawableH);
	}
	if (g_drawableW <= 0 || g_drawableH <= 0) {
		return;
	}
	if (FindFinger(p_finger.touchID, p_finger.fingerID) || g_fingerCount == (int) SDL_arraysize(g_fingers)) {
		return;
	}

	TouchFinger& finger = g_fingers[g_fingerCount++];
	finger.m_touchId = p_finger.touchID;
	finger.m_fingerId = p_finger.fingerID;
	finger.m_role = TouchRole::Ignored;
	finger.m_button = c_touchButtonNone;
	finger.m_originNX = p_finger.x;
	finger.m_originNY = p_finger.y;
	finger.m_nx = p_finger.x;
	finger.m_ny = p_finger.y;
	finger.m_fromMouse = fromMouse;

	TouchLayout layout;
	ComputeLayout(layout);
	float px = p_finger.x * (float) g_drawableW;
	float py = p_finger.y * (float) g_drawableH;

	switch (CurrentMode()) {
	case TouchMode::Race: {
		TouchButton hit = HitTestButtons(layout, px, py, c_touchButtonPause, c_touchButtonBrake);
		if (hit != c_touchButtonNone) {
			finger.m_role = TouchRole::Button;
			finger.m_button = hit;
			if (hit == c_touchButtonPause) {
				PushKeyEvent(SDL_SCANCODE_ESCAPE, true);
			}
		}
		else if (px < (float) g_drawableW * 0.5f && !RoleActive(TouchRole::Steer)) {
			finger.m_role = TouchRole::Steer;
		}
		break;
	}
	case TouchMode::RaceDialog: {
		TouchButton hit = HitTestButtons(layout, px, py, c_touchButtonDialogUp, c_touchButtonDialogDown);
		if (hit != c_touchButtonNone) {
			finger.m_role = TouchRole::DialogButton;
			finger.m_button = hit;
			PushKeyEvent(ButtonScancode(hit), true);
		}
		break;
	}
	case TouchMode::Menu:
		BecomeMenuCursor(finger);
		break;
	case TouchMode::Disabled:
		break;
	}
}

static void HandleFingerMotion(const SDL_TouchFingerEvent& p_finger)
{
	TouchFinger* finger = FindFinger(p_finger.touchID, p_finger.fingerID);
	if (!finger) {
		return;
	}
	finger->m_nx = p_finger.x;
	finger->m_ny = p_finger.y;
	if (finger->m_role == TouchRole::MenuCursor) {
		g_menuCursorNX = p_finger.x;
		g_menuCursorNY = p_finger.y;
		// Keep the hover pass tracking the moving finger (see BecomeMenuCursor).
		MiniwinInput_InjectMouseMotion(1, 0);
	}
}

static void HandleFingerUp(const SDL_TouchFingerEvent& p_finger, bool p_canceled)
{
	TouchFinger* finger = FindFinger(p_finger.touchID, p_finger.fingerID);
	if (!finger) {
		return;
	}
	finger->m_nx = p_finger.x;
	finger->m_ny = p_finger.y;

	switch (finger->m_role) {
	case TouchRole::Button:
		if (finger->m_button == c_touchButtonPause) {
			PushKeyEvent(SDL_SCANCODE_ESCAPE, false);
		}
		break;
	case TouchRole::DialogButton:
		PushKeyEvent(ButtonScancode(finger->m_button), false);
		break;
	case TouchRole::MenuCursor:
		g_menuCursorNX = p_finger.x;
		g_menuCursorNY = p_finger.y;
		g_menuCursorDown = false;
		g_menuCursorUpAtMs = SDL_GetTicks();
		// Click on release, once the hover pass has armed the widget under the
		// finger (see BecomeMenuCursor). A canceled finger releases without a click.
		if (!p_canceled) {
			MiniwinInput_InjectMouseButton(true);
			MiniwinInput_InjectMouseButton(false);
		}
		break;
	default:
		break;
	}

	*finger = g_fingers[--g_fingerCount];
}

// --- Public API ---

void MiniwinTouch_HandleEvent(const SDL_Event& p_event)
{
	switch (p_event.type) {
	case SDL_EVENT_FINGER_DOWN:
		HandleFingerDown(p_event.tfinger);
		break;
	case SDL_EVENT_FINGER_MOTION:
		HandleFingerMotion(p_event.tfinger);
		break;
	case SDL_EVENT_FINGER_UP:
		HandleFingerUp(p_event.tfinger, false);
		break;
	case SDL_EVENT_FINGER_CANCELED:
		HandleFingerUp(p_event.tfinger, true);
		break;
	default:
		break;
	}
}

void MiniwinTouch_ClaimPlayer(void* p_owner)
{
	if (!g_playerOwner && p_owner) {
		g_playerOwner = p_owner;
		g_prevButtons = 0;
		g_raceGateOpen = false;
	}
}

void MiniwinTouch_ReleasePlayer(void* p_owner)
{
	if (g_playerOwner && g_playerOwner == p_owner) {
		g_playerOwner = nullptr;
		g_prevButtons = 0;
		g_raceGateOpen = false;
		g_dialogPending = false; // a dialog cannot outlive its session
	}
}

bool MiniwinTouch_GetSteer(void* p_owner, float* p_steer)
{
	if (!p_owner || p_owner != g_playerOwner || g_drawableW <= 0 || g_drawableH <= 0) {
		return false;
	}
	for (int i = 0; i < g_fingerCount; i++) {
		const TouchFinger& finger = g_fingers[i];
		if (finger.m_role != TouchRole::Steer) {
			continue;
		}
		float unit = (float) (g_drawableW < g_drawableH ? g_drawableW : g_drawableH);
		float radius = g_steerRadiusFrac * unit;
		float deltaPx = (finger.m_nx - finger.m_originNX) * (float) g_drawableW;
		// Game analog convention: positive steers left (the negated device axis).
		float steer = -deltaPx / radius;
		if (steer > 1.0f) {
			steer = 1.0f;
		}
		else if (steer < -1.0f) {
			steer = -1.0f;
		}
		*p_steer = steer;
		return true;
	}
	return false;
}

bool MiniwinTouch_PollRaceButtons(void* p_owner, bool p_gate, unsigned* p_pressed, unsigned* p_released)
{
	*p_pressed = 0;
	*p_released = 0;
	if (!p_owner || p_owner != g_playerOwner) {
		return false;
	}
	if (!p_gate) {
		// Control is disabled (countdown, pause dialog, crash/respawn takeover):
		// nothing is reported, and taps fully within the window stay swallowed.
		g_raceGateOpen = false;
		return false;
	}
	if (!g_raceGateOpen) {
		// Control just returned (GO, pause CONTINUE, recovery): the game reset its
		// input flags while the gate was closed (ReleaseAllInputs), so mirror from
		// scratch — every still-held button re-fires its press edge, and releases
		// that happened inside the window need no edge.
		g_raceGateOpen = true;
		g_prevButtons = 0;
	}
	g_dialogPending = false; // race ticking again means any dialog is gone
	unsigned current = ComputeButtons();
	*p_pressed = current & ~g_prevButtons;
	*p_released = g_prevButtons & ~current;
	g_prevButtons = current;
	return (*p_pressed | *p_released) != 0;
}

void MiniwinTouch_NotifyRaceDialog()
{
	g_dialogPending = true;
}

void MiniwinTouch_SetLocalPlayerCount(unsigned p_count)
{
	g_multiPlayerSession = p_count > 1;
}

#ifdef __EMSCRIPTEN__
// SDL's emscripten backend has no screen-keyboard support, so a hidden <input> in
// shell.html summons the mobile keyboard. Its own function keeps the EM_ASM JS braces
// from confusing clang-format (same pattern as main.cpp's fullscreen stub).
static void OskFocusBrowser(bool p_focused)
{
	// clang-format off
	MAIN_THREAD_EM_ASM({ if (Module.racersOskFocus) Module.racersOskFocus($0); }, p_focused ? 1 : 0);
	// clang-format on
}

// Called from shell.html's hidden input on the browser thread: printable ASCII rides
// SDL text input, backspace (8) and return (13) ride key events — all through the
// mutex-protected miniwin queue, exactly like real input.
extern "C" EMSCRIPTEN_KEEPALIVE void racers_osk_key(int p_char)
{
	if (p_char == 8) {
		PushKeyEvent(SDL_SCANCODE_BACKSPACE, true);
		PushKeyEvent(SDL_SCANCODE_BACKSPACE, false);
	}
	else if (p_char == 13) {
		PushKeyEvent(SDL_SCANCODE_RETURN, true);
		PushKeyEvent(SDL_SCANCODE_RETURN, false);
	}
	else if (p_char >= 32 && p_char < 127) {
		char text[2] = {(char) p_char, '\0'};
		SDL_Event event;
		SDL_zero(event);
		event.type = SDL_EVENT_TEXT_INPUT;
		event.text.text = text; // the queue deep-copies text-input strings
		MiniwinApp_PushEvent(event);
	}
}
#endif

void MiniwinTouch_TextFieldFocus(bool p_focused)
{
	// Only ever start SDL text input where touch is actually in use: staying an input
	// method client has caused intermittent macOS window-server stalls during races,
	// so desktops without touch keep the existing key-event-only path.
	if (!g_everTouched || !g_window) {
		g_textFieldFocused = false;
		return;
	}
	g_textFieldFocused = p_focused;
	SDL_Window* window = g_window;
	if (p_focused) {
		MiniwinApp_RunOnMainThread([window]() { SDL_StartTextInput(window); });
	}
	else {
		MiniwinApp_RunOnMainThread([window]() { SDL_StopTextInput(window); });
	}
#ifdef __EMSCRIPTEN__
	OskFocusBrowser(p_focused);
#endif
}

bool MiniwinTouch_TextInputActive()
{
	return g_textFieldFocused;
}

bool MiniwinTouch_MenuCursorActive()
{
	if (g_menuCursorDown) {
		return true;
	}
	return g_menuCursorUpAtMs && SDL_GetTicks() - g_menuCursorUpAtMs <= g_menuCursorGraceMs;
}

bool MiniwinTouch_GetMenuCursor(long* p_x, long* p_y, int p_logicalW, int p_logicalH)
{
	if (!MiniwinTouch_MenuCursorActive() || p_logicalW <= 0 || p_logicalH <= 0) {
		return false;
	}

	// Map the normalized finger position into the game's logical cursor space by
	// inverting the present letterbox: menu pixels only exist inside the backend's
	// viewport rect, so a plain window scale would drift on letterboxed displays
	// (stretch and matched-aspect windows degenerate to the plain scale).
	float lx;
	float ly;
	MiniwinRenderBackend* backend = MiniwinBackend_Get();
	if (backend && backend->m_drawableW > 0 && backend->m_drawableH > 0 && backend->m_vpW > 0 && backend->m_vpH > 0) {
		float px = g_menuCursorNX * (float) backend->m_drawableW;
		float py = g_menuCursorNY * (float) backend->m_drawableH;
		lx = (px - (float) backend->m_vpX) * (float) p_logicalW / (float) backend->m_vpW;
		ly = (py - (float) backend->m_vpY) * (float) p_logicalH / (float) backend->m_vpH;
	}
	else {
		lx = g_menuCursorNX * (float) p_logicalW;
		ly = g_menuCursorNY * (float) p_logicalH;
	}

	if (lx < 0.0f) {
		lx = 0.0f;
	}
	else if (lx > (float) (p_logicalW - 1)) {
		lx = (float) (p_logicalW - 1);
	}
	if (ly < 0.0f) {
		ly = 0.0f;
	}
	else if (ly > (float) (p_logicalH - 1)) {
		ly = (float) (p_logicalH - 1);
	}
	*p_x = (long) lx;
	*p_y = (long) ly;
	return true;
}

// --- Overlay draw list (see touchoverlay.h; called from the backend Present) ---

static bool ButtonHeld(TouchButton p_button)
{
	for (int i = 0; i < g_fingerCount; i++) {
		if ((g_fingers[i].m_role == TouchRole::Button || g_fingers[i].m_role == TouchRole::DialogButton) &&
			g_fingers[i].m_button == p_button) {
			return true;
		}
	}
	return false;
}

static void FillOverlayButton(
	MiniwinTouchOverlayState* p_state,
	const TouchLayout& p_layout,
	TouchButton p_button,
	MiniwinTouchIcon p_icon
)
{
	MiniwinTouchOverlayButton& button = p_state->m_buttons[p_state->m_buttonCount++];
	const TouchRect& rect = p_layout.m_buttons[p_button];
	button.m_x = rect.m_x;
	button.m_y = rect.m_y;
	button.m_w = rect.m_w;
	button.m_h = rect.m_h;
	button.m_icon = p_icon;
	button.m_pressed = ButtonHeld(p_button);
}

bool MiniwinTouch_GetOverlay(MiniwinTouchOverlayState* p_state, int p_drawableW, int p_drawableH)
{
	if (!g_everTouched || p_drawableW <= 0 || p_drawableH <= 0) {
		return false;
	}

	// Keep the hit-test space aligned with the frame being presented (covers window
	// resizes between finger events).
	g_drawableW = p_drawableW;
	g_drawableH = p_drawableH;

	TouchMode mode = CurrentMode();
	if (mode == TouchMode::Menu || mode == TouchMode::Disabled) {
		return false;
	}

	TouchLayout layout;
	ComputeLayout(layout);

	p_state->m_buttonCount = 0;
	p_state->m_steerActive = false;

	if (mode == TouchMode::Race) {
		FillOverlayButton(p_state, layout, c_touchButtonPause, MINIWIN_TOUCH_ICON_PAUSE);
		FillOverlayButton(p_state, layout, c_touchButtonLookBack, MINIWIN_TOUCH_ICON_LOOKBACK);
		FillOverlayButton(p_state, layout, c_touchButtonPowerup, MINIWIN_TOUCH_ICON_POWERUP);
		FillOverlayButton(p_state, layout, c_touchButtonSlide, MINIWIN_TOUCH_ICON_SLIDE);
		FillOverlayButton(p_state, layout, c_touchButtonBrake, MINIWIN_TOUCH_ICON_BRAKE);

		for (int i = 0; i < g_fingerCount; i++) {
			const TouchFinger& finger = g_fingers[i];
			if (finger.m_role != TouchRole::Steer) {
				continue;
			}
			float originX = finger.m_originNX * (float) g_drawableW;
			float originY = finger.m_originNY * (float) g_drawableH;
			float deltaPx = (finger.m_nx - finger.m_originNX) * (float) g_drawableW;
			if (deltaPx > layout.m_steerRadiusPx) {
				deltaPx = layout.m_steerRadiusPx;
			}
			else if (deltaPx < -layout.m_steerRadiusPx) {
				deltaPx = -layout.m_steerRadiusPx;
			}
			p_state->m_steerActive = true;
			p_state->m_steerOriginX = originX;
			p_state->m_steerOriginY = originY;
			p_state->m_steerThumbX = originX + deltaPx;
			p_state->m_steerThumbY = originY;
			p_state->m_steerRadius = layout.m_steerRadiusPx;
			break;
		}
	}
	else {
		FillOverlayButton(p_state, layout, c_touchButtonDialogUp, MINIWIN_TOUCH_ICON_DIALOG_UP);
		FillOverlayButton(p_state, layout, c_touchButtonDialogConfirm, MINIWIN_TOUCH_ICON_DIALOG_CONFIRM);
		FillOverlayButton(p_state, layout, c_touchButtonDialogDown, MINIWIN_TOUCH_ICON_DIALOG_DOWN);
	}

	return p_state->m_buttonCount > 0;
}
