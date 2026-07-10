#pragma once

// [library:input] Touch-control overlay emission. Each backend's Present calls this
// after the scene is composited into the drawable and before the swap/submit; it is a
// no-op unless the touch layer has something to draw.

class MiniwinRenderBackend;

void MiniwinOverlay_Emit(MiniwinRenderBackend* p_backend);
