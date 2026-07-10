#pragma once

// [library:input] Internal contract between the touch layer (dinput/touch.cpp) and
// the overlay renderer (d3d/backends/overlay.cpp). All coordinates and sizes are in
// drawable pixels, top-left origin.

enum MiniwinTouchIcon {
	MINIWIN_TOUCH_ICON_PAUSE = 0,
	MINIWIN_TOUCH_ICON_LOOKBACK,
	MINIWIN_TOUCH_ICON_POWERUP,
	MINIWIN_TOUCH_ICON_SLIDE,
	MINIWIN_TOUCH_ICON_BRAKE,
	MINIWIN_TOUCH_ICON_DIALOG_UP,
	MINIWIN_TOUCH_ICON_DIALOG_CONFIRM,
	MINIWIN_TOUCH_ICON_DIALOG_DOWN,
};

struct MiniwinTouchOverlayButton {
	float m_x;
	float m_y;
	float m_w;
	float m_h;
	int m_icon; // MiniwinTouchIcon
	bool m_pressed;
};

struct MiniwinTouchOverlayState {
	int m_buttonCount;
	MiniwinTouchOverlayButton m_buttons[8];
	bool m_steerActive; // draw the steering origin ring + thumb dot
	float m_steerOriginX;
	float m_steerOriginY;
	float m_steerThumbX;
	float m_steerThumbY;
	float m_steerRadius;
};

// Fills the overlay draw list for the current touch mode and refreshes the touch
// layer's drawable-size snapshot (keeping hit-tests aligned with the presented
// frame). Returns false when nothing should be drawn — no touch device used yet,
// menu mode, or movie playback.
bool MiniwinTouch_GetOverlay(MiniwinTouchOverlayState* p_state, int p_drawableW, int p_drawableH);
