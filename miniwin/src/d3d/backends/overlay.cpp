// [library:input] Touch-control overlay renderer: builds one alpha-blended,
// untextured triangle batch (button plates + geometric icons + steering feedback) in
// drawable-pixel space from the touch layer's draw list and hands it to the active
// backend's DrawOverlay. Pure vertex-color geometry — no textures, no fonts — so a
// single implementation serves every backend and nothing needs a lifecycle.

#include "overlay.h"

#include "renderbackend.h"
#include "touchoverlay.h"

#include <SDL3/SDL.h>

// One frame's worth of overlay geometry (a handful of buttons; generously capped).
static D3DTLVERTEX g_vertices[1024];
static Uint16 g_indices[2048];
static Uint32 g_vertexCount;
static Uint32 g_indexCount;

static Uint32 Color(float p_alpha, Uint8 p_r, Uint8 p_g, Uint8 p_b)
{
	Uint32 a = (Uint32) (p_alpha * 255.0f + 0.5f);
	return (a << 24) | ((Uint32) p_r << 16) | ((Uint32) p_g << 8) | (Uint32) p_b;
}

static void PushVertex(float p_x, float p_y, Uint32 p_color)
{
	D3DTLVERTEX& vertex = g_vertices[g_vertexCount++];
	vertex.sx = p_x;
	vertex.sy = p_y;
	vertex.sz = 0.0f;
	vertex.rhw = 1.0f;
	vertex.color = p_color;
	vertex.specular = 0;
	vertex.tu = 0.0f;
	vertex.tv = 0.0f;
}

static bool HasRoom(Uint32 p_vertices, Uint32 p_indices)
{
	return g_vertexCount + p_vertices <= SDL_arraysize(g_vertices) &&
		   g_indexCount + p_indices <= SDL_arraysize(g_indices);
}

static void PushTriangle(float p_ax, float p_ay, float p_bx, float p_by, float p_cx, float p_cy, Uint32 p_color)
{
	if (!HasRoom(3, 3)) {
		return;
	}
	Uint16 base = (Uint16) g_vertexCount;
	PushVertex(p_ax, p_ay, p_color);
	PushVertex(p_bx, p_by, p_color);
	PushVertex(p_cx, p_cy, p_color);
	g_indices[g_indexCount++] = base;
	g_indices[g_indexCount++] = (Uint16) (base + 1);
	g_indices[g_indexCount++] = (Uint16) (base + 2);
}

static void PushQuad(float p_x0, float p_y0, float p_x1, float p_y1, Uint32 p_color)
{
	if (!HasRoom(4, 6)) {
		return;
	}
	Uint16 base = (Uint16) g_vertexCount;
	PushVertex(p_x0, p_y0, p_color);
	PushVertex(p_x1, p_y0, p_color);
	PushVertex(p_x1, p_y1, p_color);
	PushVertex(p_x0, p_y1, p_color);
	static const Uint16 order[6] = {0, 1, 2, 0, 2, 3};
	for (int i = 0; i < 6; i++) {
		g_indices[g_indexCount++] = (Uint16) (base + order[i]);
	}
}

// A thick line segment as a quad (icons are strokes: arrows, skid marks, checkmark).
static void PushLine(float p_x0, float p_y0, float p_x1, float p_y1, float p_thickness, Uint32 p_color)
{
	if (!HasRoom(4, 6)) {
		return;
	}
	float dx = p_x1 - p_x0;
	float dy = p_y1 - p_y0;
	float length = SDL_sqrtf(dx * dx + dy * dy);
	if (length <= 0.0001f) {
		return;
	}
	float nx = -dy / length * p_thickness * 0.5f;
	float ny = dx / length * p_thickness * 0.5f;
	Uint16 base = (Uint16) g_vertexCount;
	PushVertex(p_x0 + nx, p_y0 + ny, p_color);
	PushVertex(p_x1 + nx, p_y1 + ny, p_color);
	PushVertex(p_x1 - nx, p_y1 - ny, p_color);
	PushVertex(p_x0 - nx, p_y0 - ny, p_color);
	static const Uint16 order[6] = {0, 1, 2, 0, 2, 3};
	for (int i = 0; i < 6; i++) {
		g_indices[g_indexCount++] = (Uint16) (base + order[i]);
	}
}

// A filled regular polygon (button plates, the brake octagon, the steering thumb).
static void PushNgon(float p_cx, float p_cy, float p_radius, int p_sides, float p_rotation, Uint32 p_color)
{
	if (p_sides < 3 || !HasRoom((Uint32) p_sides + 1, (Uint32) p_sides * 3)) {
		return;
	}
	Uint16 center = (Uint16) g_vertexCount;
	PushVertex(p_cx, p_cy, p_color);
	for (int i = 0; i < p_sides; i++) {
		float angle = p_rotation + (float) i * (2.0f * SDL_PI_F / (float) p_sides);
		PushVertex(p_cx + SDL_cosf(angle) * p_radius, p_cy + SDL_sinf(angle) * p_radius, p_color);
	}
	for (int i = 0; i < p_sides; i++) {
		g_indices[g_indexCount++] = center;
		g_indices[g_indexCount++] = (Uint16) (center + 1 + i);
		g_indices[g_indexCount++] = (Uint16) (center + 1 + (i + 1) % p_sides);
	}
}

// An unfilled circle from line segments (the steering origin ring).
static void PushRing(float p_cx, float p_cy, float p_radius, float p_thickness, int p_segments, Uint32 p_color)
{
	float prevX = p_cx + p_radius;
	float prevY = p_cy;
	for (int i = 1; i <= p_segments; i++) {
		float angle = (float) i * (2.0f * SDL_PI_F / (float) p_segments);
		float x = p_cx + SDL_cosf(angle) * p_radius;
		float y = p_cy + SDL_sinf(angle) * p_radius;
		PushLine(prevX, prevY, x, y, p_thickness, p_color);
		prevX = x;
		prevY = y;
	}
}

static void PushIcon(const MiniwinTouchOverlayButton& p_button, Uint32 p_color)
{
	// Icon strokes live in a content box inset from the button plate.
	float inset = p_button.m_w * 0.28f;
	float x = p_button.m_x + inset;
	float y = p_button.m_y + inset;
	float w = p_button.m_w - inset * 2.0f;
	float h = p_button.m_h - inset * 2.0f;
	float stroke = p_button.m_w * 0.09f;

	switch (p_button.m_icon) {
	case MINIWIN_TOUCH_ICON_PAUSE:
		PushQuad(x + w * 0.18f, y, x + w * 0.42f, y + h, p_color);
		PushQuad(x + w * 0.58f, y, x + w * 0.82f, y + h, p_color);
		break;
	case MINIWIN_TOUCH_ICON_LOOKBACK:
		// U-turn arrow: up the right side, across the top, down-left into an arrowhead.
		PushLine(x + w * 0.82f, y + h, x + w * 0.82f, y + h * 0.22f, stroke, p_color);
		PushLine(x + w * 0.86f, y + h * 0.18f, x + w * 0.14f, y + h * 0.18f, stroke, p_color);
		PushLine(x + w * 0.18f, y + h * 0.14f, x + w * 0.18f, y + h * 0.62f, stroke, p_color);
		PushTriangle(x, y + h * 0.60f, x + w * 0.36f, y + h * 0.60f, x + w * 0.18f, y + h, p_color);
		break;
	case MINIWIN_TOUCH_ICON_POWERUP:
		// A power-up brick: two studs on a body.
		PushQuad(x + w * 0.12f, y + h * 0.06f, x + w * 0.38f, y + h * 0.34f, p_color);
		PushQuad(x + w * 0.62f, y + h * 0.06f, x + w * 0.88f, y + h * 0.34f, p_color);
		PushQuad(x, y + h * 0.34f, x + w, y + h, p_color);
		break;
	case MINIWIN_TOUCH_ICON_SLIDE:
		// Skid marks.
		PushLine(x + w * 0.30f, y + h, x + w * 0.55f, y, stroke * 1.4f, p_color);
		PushLine(x + w * 0.65f, y + h, x + w * 0.90f, y, stroke * 1.4f, p_color);
		break;
	case MINIWIN_TOUCH_ICON_BRAKE:
		// A stop-sign octagon, flat edge up.
		PushNgon(
			p_button.m_x + p_button.m_w * 0.5f,
			p_button.m_y + p_button.m_h * 0.5f,
			w * 0.62f,
			8,
			SDL_PI_F / 8.0f,
			p_color
		);
		break;
	case MINIWIN_TOUCH_ICON_DIALOG_UP:
		PushTriangle(x + w * 0.5f, y, x + w, y + h, x, y + h, p_color);
		break;
	case MINIWIN_TOUCH_ICON_DIALOG_DOWN:
		PushTriangle(x, y, x + w, y, x + w * 0.5f, y + h, p_color);
		break;
	case MINIWIN_TOUCH_ICON_DIALOG_CONFIRM:
		PushLine(x, y + h * 0.55f, x + w * 0.38f, y + h * 0.92f, stroke * 1.3f, p_color);
		PushLine(x + w * 0.38f, y + h * 0.92f, x + w, y + h * 0.10f, stroke * 1.3f, p_color);
		break;
	default:
		break;
	}
}

void MiniwinOverlay_Emit(MiniwinRenderBackend* p_backend)
{
	if (!p_backend || p_backend->m_drawableW <= 0 || p_backend->m_drawableH <= 0) {
		return;
	}

	MiniwinTouchOverlayState state;
	if (!MiniwinTouch_GetOverlay(&state, p_backend->m_drawableW, p_backend->m_drawableH)) {
		return;
	}

	g_vertexCount = 0;
	g_indexCount = 0;

	for (int i = 0; i < state.m_buttonCount; i++) {
		const MiniwinTouchOverlayButton& button = state.m_buttons[i];
		float plateAlpha = button.m_pressed ? 0.55f : 0.35f;
		float iconAlpha = button.m_pressed ? 0.95f : 0.75f;

		Uint32 iconColor;
		switch (button.m_icon) {
		case MINIWIN_TOUCH_ICON_BRAKE:
			iconColor = Color(iconAlpha, 0xE0, 0x30, 0x28); // stop red
			break;
		case MINIWIN_TOUCH_ICON_POWERUP:
			iconColor = Color(iconAlpha, 0x40, 0xD0, 0x40); // boost-brick green
			break;
		case MINIWIN_TOUCH_ICON_SLIDE:
			iconColor = Color(iconAlpha, 0xF0, 0xB0, 0x30); // skid amber
			break;
		default:
			iconColor = Color(iconAlpha, 0xF0, 0xF0, 0xF0);
			break;
		}

		PushNgon(
			button.m_x + button.m_w * 0.5f,
			button.m_y + button.m_h * 0.5f,
			button.m_w * 0.5f,
			12,
			SDL_PI_F / 12.0f,
			Color(plateAlpha, 0x14, 0x14, 0x14)
		);
		PushIcon(button, iconColor);
	}

	if (state.m_steerActive) {
		Uint32 ringColor = Color(0.30f, 0xF0, 0xF0, 0xF0);
		PushRing(
			state.m_steerOriginX,
			state.m_steerOriginY,
			state.m_steerRadius,
			state.m_steerRadius * 0.06f,
			20,
			ringColor
		);
		PushNgon(
			state.m_steerThumbX,
			state.m_steerThumbY,
			state.m_steerRadius * 0.28f,
			12,
			0.0f,
			Color(0.45f, 0xF0, 0xF0, 0xF0)
		);
	}

	if (g_vertexCount && g_indexCount) {
		p_backend->DrawOverlay(g_vertices, g_vertexCount, g_indices, g_indexCount);
	}
}
