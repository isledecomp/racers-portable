// [library:3d] OpenGL 3.3 core render backend.

#include "gl3loader.h"
#include "miniwin.h"
#include "renderbackend.h"

#include <stdlib.h>
#include <string.h>

static const char* g_vertexShader = R"GLSL(#version 330 core
layout(location = 0) in vec4 aPos;      // sx, sy, sz, rhw
layout(location = 1) in vec4 aColor;    // D3DCOLOR diffuse (BGRA attribute)
layout(location = 2) in vec4 aSpecular; // D3DCOLOR specular
layout(location = 3) in vec2 aUV;

uniform vec2 uViewport; // logical render size (e.g. 640x480)

out vec4 vColor;
out vec4 vSpecular;
out vec2 vUV;

void main()
{
	// D3D TL vertices are in screen space with rhw = 1/w. Reconstruct clip space so
	// the hardware perspective-corrects colors/UVs exactly like Direct3D 6 did.
	float w = 1.0 / aPos.w;
	float x = (aPos.x * 2.0 / uViewport.x - 1.0) * w;
	float y = (1.0 - aPos.y * 2.0 / uViewport.y) * w;
	float z = (aPos.z * 2.0 - 1.0) * w;
	gl_Position = vec4(x, y, z, w);
	vColor = aColor;
	vSpecular = aSpecular;
	vUV = aUV;
}
)GLSL";

static const char* g_fragmentShader = R"GLSL(#version 330 core
in vec4 vColor;
in vec4 vSpecular;
in vec2 vUV;

uniform sampler2D uTexture;
uniform int uColorOp;  // 0 = texture * diffuse, 1 = texture, 2 = diffuse
uniform int uAlphaOp;  // same encoding for the alpha channel
uniform int uSpecular; // D3DRENDERSTATE_SPECULARENABLE
uniform int uAlphaFunc; // D3DCMPFUNC or 0 = disabled
uniform float uAlphaRef;
uniform int uColorKey; // discard fragments whose sampled texture alpha is keyed out

out vec4 oColor;

void main()
{
	vec4 sampled = vec4(1.0);
	if (uColorOp != 2 || uAlphaOp != 2) {
		sampled = texture(uTexture, vUV);
	}

	if (uColorKey != 0 && sampled.a < 0.5) {
		discard;
	}

	vec4 color;
	color.rgb = uColorOp == 0 ? vColor.rgb * sampled.rgb : (uColorOp == 1 ? sampled.rgb : vColor.rgb);
	color.a = uAlphaOp == 0 ? vColor.a * sampled.a : (uAlphaOp == 1 ? sampled.a : vColor.a);

	if (uSpecular != 0) {
		color.rgb += vSpecular.rgb;
	}

	if (uAlphaFunc != 0) {
		float a = color.a;
		bool pass = true;
		if (uAlphaFunc == 1) { pass = false; }
		else if (uAlphaFunc == 2) { pass = a < uAlphaRef; }
		else if (uAlphaFunc == 3) { pass = a == uAlphaRef; }
		else if (uAlphaFunc == 4) { pass = a <= uAlphaRef; }
		else if (uAlphaFunc == 5) { pass = a > uAlphaRef; }
		else if (uAlphaFunc == 6) { pass = a != uAlphaRef; }
		else if (uAlphaFunc == 7) { pass = a >= uAlphaRef; }
		if (!pass) {
			discard;
		}
	}

	oColor = color;
}
)GLSL";

class MiniwinGl3Backend : public MiniwinRenderBackend {
public:
	bool Init(SDL_Window* p_window) override;
	void Resize(int p_width, int p_height) override;
	Uint32 CreateTexture(int p_width, int p_height, const void* p_rgba, bool p_mipmaps) override;
	void UpdateTexture(Uint32 p_id, int p_width, int p_height, const void* p_rgba, bool p_mipmaps) override;
	void DestroyTexture(Uint32 p_id) override;
	void BeginScene() override;
	void Clear(const SDL_Rect* p_rect, float p_r, float p_g, float p_b, bool p_color, bool p_depth) override;
	void DrawTriangles(
		const MiniwinRasterState& p_state,
		const D3DTLVERTEX* p_vertices,
		Uint32 p_vertexCount,
		const Uint16* p_indices,
		Uint32 p_indexCount
	) override;
	void EndScene() override;
	void Present() override;
	SDL_Surface* ReadBackbuffer() override;

private:
	GLuint CompileShader(GLenum p_type, const char* p_source);
	void ApplyState(const MiniwinRasterState& p_state);
	static GLenum BlendFactor(D3DBLEND p_blend);
	static GLenum CompareFunc(D3DCMPFUNC p_func);
	void HandleFrameDump();

	SDL_Window* m_window = nullptr;
	SDL_GLContext m_context = nullptr;
	Gl3Functions gl;
	GLuint m_program = 0;
	GLuint m_vao = 0;
	GLuint m_vbo = 0;
	GLuint m_ebo = 0;
	GLint m_uViewport = -1;
	GLint m_uColorOp = -1;
	GLint m_uAlphaOp = -1;
	GLint m_uSpecular = -1;
	GLint m_uAlphaFunc = -1;
	GLint m_uColorKey = -1;
	GLint m_uAlphaRef = -1;
	Uint64 m_frameCounter = 0;
	Uint32 m_statDraws = 0;
	Uint32 m_statVertices = 0;
	Uint32 m_statClears = 0;
	bool m_statLoggedSample = false;
};

Uint32 MiniwinBackend_PrepareWindowFlags()
{
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	return SDL_WINDOW_OPENGL;
}

MiniwinRenderBackend* MiniwinBackend_Create(SDL_Window* p_window, int p_width, int p_height)
{
	MiniwinGl3Backend* backend = new MiniwinGl3Backend();
	backend->m_width = p_width;
	backend->m_height = p_height;
	if (!backend->Init(p_window)) {
		delete backend;
		return nullptr;
	}
	return backend;
}

GLuint MiniwinGl3Backend::CompileShader(GLenum p_type, const char* p_source)
{
	GLuint shader = gl.glCreateShader(p_type);
	gl.glShaderSource(shader, 1, &p_source, nullptr);
	gl.glCompileShader(shader);

	GLint status = 0;
	gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char log[1024];
		gl.glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
		SDL_LogError(LOG_CATEGORY_MINIWIN, "Shader compile failed: %s", log);
		return 0;
	}

	return shader;
}

bool MiniwinGl3Backend::Init(SDL_Window* p_window)
{
	m_window = p_window;

	m_context = SDL_GL_CreateContext(p_window);
	if (!m_context) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_GL_CreateContext failed: %s", SDL_GetError());
		return false;
	}

	if (!SDL_GL_MakeCurrent(p_window, m_context)) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_GL_MakeCurrent failed: %s", SDL_GetError());
		return false;
	}

	if (!gl.LoadAll()) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "Failed to load OpenGL 3.3 functions");
		return false;
	}

	SDL_LogInfo(LOG_CATEGORY_MINIWIN, "OpenGL renderer: %s", (const char*) gl.glGetString(GL_VERSION));
	SDL_GL_SetSwapInterval(0); // the game has its own frame limiter

	GLuint vs = CompileShader(GL_VERTEX_SHADER, g_vertexShader);
	GLuint fs = CompileShader(GL_FRAGMENT_SHADER, g_fragmentShader);
	if (!vs || !fs) {
		return false;
	}

	m_program = gl.glCreateProgram();
	gl.glAttachShader(m_program, vs);
	gl.glAttachShader(m_program, fs);
	gl.glLinkProgram(m_program);

	GLint status = 0;
	gl.glGetProgramiv(m_program, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1024];
		gl.glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
		SDL_LogError(LOG_CATEGORY_MINIWIN, "Program link failed: %s", log);
		return false;
	}

	gl.glDeleteShader(vs);
	gl.glDeleteShader(fs);

	m_uViewport = gl.glGetUniformLocation(m_program, "uViewport");
	m_uColorOp = gl.glGetUniformLocation(m_program, "uColorOp");
	m_uAlphaOp = gl.glGetUniformLocation(m_program, "uAlphaOp");
	m_uSpecular = gl.glGetUniformLocation(m_program, "uSpecular");
	m_uAlphaFunc = gl.glGetUniformLocation(m_program, "uAlphaFunc");
	m_uColorKey = gl.glGetUniformLocation(m_program, "uColorKey");
	m_uAlphaRef = gl.glGetUniformLocation(m_program, "uAlphaRef");

	gl.glGenVertexArrays(1, &m_vao);
	gl.glBindVertexArray(m_vao);
	gl.glGenBuffers(1, &m_vbo);
	gl.glGenBuffers(1, &m_ebo);
	gl.glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);

	const GLsizei stride = sizeof(D3DTLVERTEX);
	gl.glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void*) offsetof(D3DTLVERTEX, sx));
	gl.glEnableVertexAttribArray(0);
	gl.glVertexAttribPointer(1, GL_BGRA, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*) offsetof(D3DTLVERTEX, color));
	gl.glEnableVertexAttribArray(1);
	gl.glVertexAttribPointer(2, GL_BGRA, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*) offsetof(D3DTLVERTEX, specular));
	gl.glEnableVertexAttribArray(2);
	gl.glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*) offsetof(D3DTLVERTEX, tu));
	gl.glEnableVertexAttribArray(3);

	gl.glUseProgram(m_program);
	gl.glUniform2f(m_uViewport, (GLfloat) m_width, (GLfloat) m_height);
	gl.glUniform1i(gl.glGetUniformLocation(m_program, "uTexture"), 0);
	gl.glActiveTexture(GL_TEXTURE0);

	gl.glEnable(GL_DEPTH_TEST);
	gl.glDepthFunc(GL_LEQUAL);
	gl.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	gl.glPixelStorei(GL_PACK_ALIGNMENT, 1);

	int dw = 0;
	int dh = 0;
	SDL_GetWindowSizeInPixels(m_window, &dw, &dh);
	gl.glViewport(0, 0, dw, dh);

	return true;
}

void MiniwinGl3Backend::Resize(int p_width, int p_height)
{
	m_width = p_width;
	m_height = p_height;
	gl.glUseProgram(m_program);
	gl.glUniform2f(m_uViewport, (GLfloat) m_width, (GLfloat) m_height);

	int dw = 0;
	int dh = 0;
	SDL_GetWindowSizeInPixels(m_window, &dw, &dh);
	gl.glViewport(0, 0, dw, dh);
}

Uint32 MiniwinGl3Backend::CreateTexture(int p_width, int p_height, const void* p_rgba, bool p_mipmaps)
{
	GLuint id = 0;
	gl.glGenTextures(1, &id);
	UpdateTexture(id, p_width, p_height, p_rgba, p_mipmaps);
	return id;
}

Uint32 g_miniwinStatTexUploads;
Uint64 g_miniwinStatTexUploadBytes;

void MiniwinGl3Backend::UpdateTexture(Uint32 p_id, int p_width, int p_height, const void* p_rgba, bool p_mipmaps)
{
	MiniwinPhaseScope phase(MINIWIN_PHASE_TEXTURE_UPLOAD);
	g_miniwinStatTexUploads++;
	g_miniwinStatTexUploadBytes += (Uint64) p_width * p_height * 4;
	char detail[64];
	SDL_snprintf(detail, sizeof(detail), "tex %u %dx%d", (unsigned) p_id, p_width, p_height);
	MiniwinSlowOpLog slowLog("UpdateTexture", detail);

	static const char* dumpDir = getenv("RACERS_DUMP_TEX");
	if (dumpDir && p_id <= 64) {
		SDL_Surface* surface =
			SDL_CreateSurfaceFrom(p_width, p_height, SDL_PIXELFORMAT_RGBA32, const_cast<void*>(p_rgba), p_width * 4);
		if (surface) {
			char path[512];
			SDL_snprintf(path, sizeof(path), "%s/tex%02u_%dx%d.bmp", dumpDir, (unsigned) p_id, p_width, p_height);
			SDL_SaveBMP(surface, path);
			SDL_DestroySurface(surface);
		}
	}

	gl.glBindTexture(GL_TEXTURE_2D, p_id);
	gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, p_width, p_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, p_rgba);
	if (p_mipmaps) {
		gl.glGenerateMipmap(GL_TEXTURE_2D);
	}
}

void MiniwinGl3Backend::DestroyTexture(Uint32 p_id)
{
	GLuint id = p_id;
	gl.glDeleteTextures(1, &id);
}

void MiniwinGl3Backend::BeginScene()
{
}

void MiniwinGl3Backend::Clear(const SDL_Rect* p_rect, float p_r, float p_g, float p_b, bool p_color, bool p_depth)
{
	int dw = 0;
	int dh = 0;
	SDL_GetWindowSizeInPixels(m_window, &dw, &dh);

	bool scissor = false;
	if (p_rect && (p_rect->x != 0 || p_rect->y != 0 || p_rect->w != m_width || p_rect->h != m_height)) {
		float sx = (float) dw / (float) m_width;
		float sy = (float) dh / (float) m_height;
		int x = (int) (p_rect->x * sx);
		int w = (int) (p_rect->w * sx);
		int h = (int) (p_rect->h * sy);
		int y = dh - (int) ((p_rect->y + p_rect->h) * sy);
		gl.glEnable(GL_SCISSOR_TEST);
		gl.glScissor(x, y, w, h);
		scissor = true;
	}

	m_statClears++;

	GLbitfield mask = 0;
	if (p_color) {
		gl.glClearColor(p_r, p_g, p_b, 1.0f);
		mask |= GL_COLOR_BUFFER_BIT;
	}
	if (p_depth) {
		gl.glDepthMask(GL_TRUE);
		gl.glClearDepth(1.0);
		mask |= GL_DEPTH_BUFFER_BIT;
	}
	gl.glClear(mask);

	if (scissor) {
		gl.glDisable(GL_SCISSOR_TEST);
	}
}

GLenum MiniwinGl3Backend::BlendFactor(D3DBLEND p_blend)
{
	switch (p_blend) {
	case D3DBLEND_ZERO:
		return GL_ZERO;
	case D3DBLEND_ONE:
		return GL_ONE;
	case D3DBLEND_SRCCOLOR:
		return GL_SRC_COLOR;
	case D3DBLEND_INVSRCCOLOR:
		return GL_ONE_MINUS_SRC_COLOR;
	case D3DBLEND_SRCALPHA:
		return GL_SRC_ALPHA;
	case D3DBLEND_INVSRCALPHA:
		return GL_ONE_MINUS_SRC_ALPHA;
	case D3DBLEND_DESTALPHA:
		return GL_DST_ALPHA;
	case D3DBLEND_INVDESTALPHA:
		return GL_ONE_MINUS_DST_ALPHA;
	case D3DBLEND_DESTCOLOR:
		return GL_DST_COLOR;
	case D3DBLEND_INVDESTCOLOR:
		return GL_ONE_MINUS_DST_COLOR;
	case D3DBLEND_SRCALPHASAT:
		return GL_SRC_ALPHA_SATURATE;
	default:
		return GL_ONE;
	}
}

GLenum MiniwinGl3Backend::CompareFunc(D3DCMPFUNC p_func)
{
	switch (p_func) {
	case D3DCMP_NEVER:
		return GL_NEVER;
	case D3DCMP_LESS:
		return GL_LESS;
	case D3DCMP_EQUAL:
		return GL_EQUAL;
	case D3DCMP_LESSEQUAL:
		return GL_LEQUAL;
	case D3DCMP_GREATER:
		return GL_GREATER;
	case D3DCMP_NOTEQUAL:
		return GL_NOTEQUAL;
	case D3DCMP_GREATEREQUAL:
		return GL_GEQUAL;
	case D3DCMP_ALWAYS:
	default:
		return GL_ALWAYS;
	}
}

void MiniwinGl3Backend::ApplyState(const MiniwinRasterState& p_state)
{
	if (p_state.zEnable) {
		gl.glEnable(GL_DEPTH_TEST);
		gl.glDepthFunc(CompareFunc(p_state.zFunc));
	}
	else {
		gl.glDisable(GL_DEPTH_TEST);
	}
	gl.glDepthMask(p_state.zWrite ? GL_TRUE : GL_FALSE);

	if (p_state.alphaBlend) {
		gl.glEnable(GL_BLEND);
		gl.glBlendFunc(BlendFactor(p_state.srcBlend), BlendFactor(p_state.destBlend));
	}
	else {
		gl.glDisable(GL_BLEND);
	}

	// D3D screen-space triangles are wound for a top-left origin; our projection flips
	// Y, which reverses the winding GL sees.
	if (p_state.cullMode == D3DCULL_NONE) {
		gl.glDisable(GL_CULL_FACE);
	}
	else {
		gl.glEnable(GL_CULL_FACE);
		gl.glCullFace(GL_BACK);
		gl.glFrontFace(p_state.cullMode == D3DCULL_CW ? GL_CW : GL_CCW);
	}

	gl.glUniform1i(m_uColorOp, (int) p_state.colorOp);
	gl.glUniform1i(m_uAlphaOp, (int) p_state.alphaOp);
	gl.glUniform1i(m_uSpecular, p_state.specular ? 1 : 0);
	gl.glUniform1i(m_uAlphaFunc, p_state.alphaTest ? (int) p_state.alphaFunc : 0);
	gl.glUniform1f(m_uAlphaRef, p_state.alphaRef);
	gl.glUniform1i(m_uColorKey, p_state.colorKeyTest ? 1 : 0);

	if (p_state.textured) {
		gl.glBindTexture(GL_TEXTURE_2D, p_state.textureId);
		GLint filter = p_state.textureLinear ? GL_LINEAR : GL_NEAREST;
		gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
		gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
		GLint wrap = p_state.textureWrap ? GL_REPEAT : GL_CLAMP_TO_EDGE;
		gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
		gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
	}
}

void MiniwinGl3Backend::DrawTriangles(
	const MiniwinRasterState& p_state,
	const D3DTLVERTEX* p_vertices,
	Uint32 p_vertexCount,
	const Uint16* p_indices,
	Uint32 p_indexCount
)
{
	gl.glUseProgram(m_program);
	gl.glBindVertexArray(m_vao);
	ApplyState(p_state);

	m_statDraws++;
	m_statVertices += p_vertexCount;
	static const char* statsEnv = getenv("RACERS_GL_STATS");
	if (statsEnv && !m_statLoggedSample && p_vertexCount >= 3) {
		m_statLoggedSample = true;
		const D3DTLVERTEX& v = p_vertices[0];
		SDL_LogInfo(
			LOG_CATEGORY_MINIWIN,
			"draw sample: v0=(%.1f, %.1f, %.3f, rhw=%.4f) color=%08x tex=%u idx=%u",
			v.sx,
			v.sy,
			v.sz,
			v.rhw,
			(unsigned) v.color,
			p_state.textureId,
			p_indices ? p_indexCount : 0
		);
	}

	static const char* traceEnv = getenv("RACERS_GL_TRACE");
	if (traceEnv && m_frameCounter == (Uint64) SDL_atoi(traceEnv)) {
		float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f, minZ = 1e9f, maxZ = -1e9f;
		for (Uint32 i = 0; i < p_vertexCount; i++) {
			minX = SDL_min(minX, p_vertices[i].sx);
			maxX = SDL_max(maxX, p_vertices[i].sx);
			minY = SDL_min(minY, p_vertices[i].sy);
			maxY = SDL_max(maxY, p_vertices[i].sy);
			minZ = SDL_min(minZ, p_vertices[i].sz);
			maxZ = SDL_max(maxZ, p_vertices[i].sz);
		}
		SDL_LogInfo(
			LOG_CATEGORY_MINIWIN,
			"trace draw: vtx=%u idx=%u tex=%u(c%da%d) blend=%d(%d,%d) atest=%d z=%d zw=%d cull=%d "
			"x[%.1f..%.1f] y[%.1f..%.1f] z[%.3f..%.3f] rhw0=%.4f c0=%08x c1=%08x uv0=(%.2f,%.2f)",
			p_vertexCount,
			p_indices ? p_indexCount : 0,
			p_state.textureId,
			(int) p_state.colorOp,
			(int) p_state.alphaOp,
			(int) p_state.alphaBlend,
			(int) p_state.srcBlend,
			(int) p_state.destBlend,
			(int) p_state.alphaTest,
			(int) p_state.zEnable,
			(int) p_state.zWrite,
			(int) p_state.cullMode,
			minX,
			maxX,
			minY,
			maxY,
			minZ,
			maxZ,
			p_vertices[0].rhw,
			(unsigned) p_vertices[0].color,
			p_vertexCount > 1 ? (unsigned) p_vertices[1].color : 0u,
			p_vertices[0].tu,
			p_vertices[0].tv
		);
	}

	gl.glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	gl.glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (p_vertexCount * sizeof(D3DTLVERTEX)), p_vertices, GL_STREAM_DRAW);

	if (p_indices) {
		gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
		gl.glBufferData(
			GL_ELEMENT_ARRAY_BUFFER,
			(GLsizeiptr) (p_indexCount * sizeof(Uint16)),
			p_indices,
			GL_STREAM_DRAW
		);
		gl.glDrawElements(GL_TRIANGLES, (GLsizei) p_indexCount, GL_UNSIGNED_SHORT, nullptr);
	}
	else {
		gl.glDrawArrays(GL_TRIANGLES, 0, (GLsizei) p_vertexCount);
	}

	if (traceEnv && m_frameCounter == (Uint64) SDL_atoi(traceEnv)) {
		GLenum err = gl.glGetError();
		if (err) {
			SDL_LogInfo(LOG_CATEGORY_MINIWIN, "trace draw: glGetError=0x%x", err);
		}

		float cx = 0.0f, cy = 0.0f;
		for (Uint32 i = 0; i < p_vertexCount; i++) {
			cx += p_vertices[i].sx;
			cy += p_vertices[i].sy;
		}
		cx /= (float) p_vertexCount;
		cy /= (float) p_vertexCount;

		int dw = 0, dh = 0;
		SDL_GetWindowSizeInPixels(m_window, &dw, &dh);
		int px = (int) (cx * dw / (float) m_width);
		int py = dh - 1 - (int) (cy * dh / (float) m_height);
		Uint8 pixel[4] = {0, 0, 0, 0};
		gl.glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
		SDL_LogInfo(
			LOG_CATEGORY_MINIWIN,
			"trace post: uv=[(%.3f,%.3f)(%.3f,%.3f)(%.3f,%.3f)(%.3f,%.3f)] idx=[%u %u %u %u %u %u] "
			"fb@(%d,%d)=%02x%02x%02x%02x",
			p_vertices[0].tu,
			p_vertices[0].tv,
			p_vertexCount > 1 ? p_vertices[1].tu : 0.f,
			p_vertexCount > 1 ? p_vertices[1].tv : 0.f,
			p_vertexCount > 2 ? p_vertices[2].tu : 0.f,
			p_vertexCount > 2 ? p_vertices[2].tv : 0.f,
			p_vertexCount > 3 ? p_vertices[3].tu : 0.f,
			p_vertexCount > 3 ? p_vertices[3].tv : 0.f,
			p_indices && p_indexCount > 0 ? p_indices[0] : 999,
			p_indices && p_indexCount > 1 ? p_indices[1] : 999,
			p_indices && p_indexCount > 2 ? p_indices[2] : 999,
			p_indices && p_indexCount > 3 ? p_indices[3] : 999,
			p_indices && p_indexCount > 4 ? p_indices[4] : 999,
			p_indices && p_indexCount > 5 ? p_indices[5] : 999,
			px,
			py,
			pixel[0],
			pixel[1],
			pixel[2],
			pixel[3]
		);

		GLint boundTexture = -1;
		gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
		SDL_LogInfo(
			LOG_CATEGORY_MINIWIN,
			"trace state: boundTex=%d spec=%d spec0=%08x",
			boundTexture,
			(int) p_state.specular,
			(unsigned) p_vertices[0].specular
		);
	}
}

void MiniwinGl3Backend::EndScene()
{
}

void MiniwinGl3Backend::HandleFrameDump()
{
	// Periodic screenshots for automated testing: RACERS_DUMP_EVERY="ms:prefix"
	// writes prefix-<seconds>.bmp every ms milliseconds.
	static const char* periodicSpec = getenv("RACERS_DUMP_EVERY");
	if (periodicSpec && periodicSpec[0]) {
		const char* colon = SDL_strchr(periodicSpec, ':');
		if (colon) {
			static Uint64 lastDumpMs;
			Uint64 intervalMs = (Uint64) SDL_atoi(periodicSpec);
			Uint64 nowMs = SDL_GetTicks();
			if (intervalMs && nowMs - lastDumpMs >= intervalMs) {
				lastDumpMs = nowMs;
				char path[512];
				SDL_snprintf(path, sizeof(path), "%s-%03llu.bmp", colon + 1, (unsigned long long) (nowMs / 1000));
				SDL_Surface* shot = ReadBackbuffer();
				if (shot) {
					SDL_SaveBMP(shot, path);
					SDL_DestroySurface(shot);
				}
			}
		}
	}

	static const char* dumpSpec = getenv("RACERS_DUMP_FRAME");
	if (!dumpSpec || !dumpSpec[0]) {
		return;
	}

	// Format: "N:path" (dump frame N to path); multiple entries separated by ','.
	char spec[512];
	SDL_strlcpy(spec, dumpSpec, sizeof(spec));

	char* saveptr = nullptr;
	for (char* entry = SDL_strtok_r(spec, ",", &saveptr); entry; entry = SDL_strtok_r(nullptr, ",", &saveptr)) {
		char* colon = SDL_strchr(entry, ':');
		if (!colon) {
			continue;
		}
		*colon = '\0';
		if ((Uint64) SDL_atoi(entry) == m_frameCounter) {
			SDL_Surface* shot = ReadBackbuffer();
			if (shot) {
				SDL_SaveBMP(shot, colon + 1);
				SDL_LogInfo(
					LOG_CATEGORY_MINIWIN,
					"Dumped frame %llu to %s",
					(unsigned long long) m_frameCounter,
					colon + 1
				);
				SDL_DestroySurface(shot);
			}
		}
	}
}

void MiniwinGl3Backend::Present()
{
	MiniwinPhaseScope phase(MINIWIN_PHASE_PRESENT);
	m_frameCounter++;

	static const char* statsEnv = getenv("RACERS_GL_STATS");
	if (statsEnv) {
		static Uint64 lastNs, minNs, maxNs, sumNs;
		Uint64 nowNs = SDL_GetTicksNS();
		if (lastNs) {
			Uint64 deltaNs = nowNs - lastNs;
			sumNs += deltaNs;
			if (!minNs || deltaNs < minNs) {
				minNs = deltaNs;
			}
			if (deltaNs > maxNs) {
				maxNs = deltaNs;
			}
		}
		lastNs = nowNs;
		if ((m_frameCounter % 100) == 0) {
			SDL_LogInfo(
				LOG_CATEGORY_MINIWIN,
				"frame time min/avg/max = %.1f/%.1f/%.1f ms",
				minNs / 1e6,
				sumNs / 100 / 1e6,
				maxNs / 1e6
			);
			minNs = maxNs = sumNs = 0;
		}
	}
	if (statsEnv && (m_frameCounter % 100) == 0) {
		SDL_LogInfo(
			LOG_CATEGORY_MINIWIN,
			"frame %llu: %u draws, %u vertices, %u clears, %u setRS, %u setTex, %u uploads (%llu KiB)",
			(unsigned long long) m_frameCounter,
			m_statDraws,
			m_statVertices,
			m_statClears,
			g_miniwinStatSetRenderState,
			g_miniwinStatSetTexture,
			g_miniwinStatTexUploads,
			(unsigned long long) (g_miniwinStatTexUploadBytes / 1024)
		);
		m_statLoggedSample = false;
	}
	m_statDraws = 0;
	m_statVertices = 0;
	m_statClears = 0;
	g_miniwinStatSetRenderState = 0;
	g_miniwinStatSetTexture = 0;
	g_miniwinStatTexUploads = 0;
	g_miniwinStatTexUploadBytes = 0;

	HandleFrameDump();

	{
		MiniwinSlowOpLog slowLog("SwapWindow", "");
		SDL_GL_SwapWindow(m_window);
	}
}

SDL_Surface* MiniwinGl3Backend::ReadBackbuffer()
{
	int dw = 0;
	int dh = 0;
	SDL_GetWindowSizeInPixels(m_window, &dw, &dh);

	SDL_Surface* surface = SDL_CreateSurface(dw, dh, SDL_PIXELFORMAT_ABGR8888);
	if (!surface) {
		return nullptr;
	}

	gl.glReadPixels(0, 0, dw, dh, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);

	// Flip vertically (GL reads bottom-up).
	int pitch = surface->pitch;
	unsigned char* pixels = (unsigned char*) surface->pixels;
	unsigned char* row = (unsigned char*) malloc(pitch);
	for (int y = 0; y < dh / 2; y++) {
		memcpy(row, pixels + y * pitch, pitch);
		memcpy(pixels + y * pitch, pixels + (dh - 1 - y) * pitch, pitch);
		memcpy(pixels + (dh - 1 - y) * pitch, row, pitch);
	}
	free(row);

	return surface;
}
