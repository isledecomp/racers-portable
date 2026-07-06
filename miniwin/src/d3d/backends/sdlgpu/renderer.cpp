// [library:3d] SDL_GPU render backend. Mirrors the OpenGL 3.3 backend's semantics
// (pre-transformed D3DTLVERTEX triangles, fixed-function texture-stage emulation, a
// persistent offscreen scene target blitted letterboxed to the window, normal-Z depth)
// on the SDL_GPU API. Shaders are precompiled bytecode (SPIR-V/MSL/DXIL) in the
// generated header.
//
// SDL_GPU has no per-draw vertex streaming and forbids copy passes inside render
// passes, so a frame's clears and draws are deferred into an ordered command list with
// their vertices/indices staged CPU-side; Present uploads them in one copy pass and
// replays the list, turning clears into render-pass boundaries. Texture uploads stay
// immediate (a separate command buffer, fenced).

#include "backends.h"
#include "miniwin.h"
#include "renderbackend.h"
#include "shaders/generated/miniwin_sdlgpu_shaders.h"

#include <miniwin/miniwinapp.h>
#include <unordered_map>
#include <vector>

namespace
{

struct VertexUniforms {
	float m_viewport[2];
	float m_pad[2];
};

struct FragmentUniforms {
	Sint32 m_colorOp;
	Sint32 m_alphaOp;
	Sint32 m_specular;
	Sint32 m_alphaFunc;
	float m_alphaRef;
	Sint32 m_colorKey;
	float m_pad[2];
};

// A deferred frame operation (see file header).
struct GpuCommand {
	enum Kind {
		Clear,
		Draw,
	};
	Kind m_kind;

	// Clear
	bool m_clearColor;
	bool m_clearDepth;
	float m_r;
	float m_g;
	float m_b;

	// Draw
	MiniwinRasterState m_state;
	Uint32 m_vertexOffset;
	Uint32 m_vertexCount;
	Uint32 m_indexOffset;
	Uint32 m_indexCount;
	bool m_indexed;
};

SDL_GPUBlendFactor BlendFactor(D3DBLEND p_blend)
{
	switch (p_blend) {
	case D3DBLEND_ZERO:
		return SDL_GPU_BLENDFACTOR_ZERO;
	case D3DBLEND_ONE:
		return SDL_GPU_BLENDFACTOR_ONE;
	case D3DBLEND_SRCCOLOR:
		return SDL_GPU_BLENDFACTOR_SRC_COLOR;
	case D3DBLEND_INVSRCCOLOR:
		return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
	case D3DBLEND_SRCALPHA:
		return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
	case D3DBLEND_INVSRCALPHA:
		return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	case D3DBLEND_DESTALPHA:
		return SDL_GPU_BLENDFACTOR_DST_ALPHA;
	case D3DBLEND_INVDESTALPHA:
		return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
	case D3DBLEND_DESTCOLOR:
		return SDL_GPU_BLENDFACTOR_DST_COLOR;
	case D3DBLEND_INVDESTCOLOR:
		return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR;
	case D3DBLEND_SRCALPHASAT:
		return SDL_GPU_BLENDFACTOR_SRC_ALPHA_SATURATE;
	default:
		return SDL_GPU_BLENDFACTOR_ONE;
	}
}

SDL_GPUCompareOp CompareOp(D3DCMPFUNC p_func)
{
	switch (p_func) {
	case D3DCMP_NEVER:
		return SDL_GPU_COMPAREOP_NEVER;
	case D3DCMP_LESS:
		return SDL_GPU_COMPAREOP_LESS;
	case D3DCMP_EQUAL:
		return SDL_GPU_COMPAREOP_EQUAL;
	case D3DCMP_LESSEQUAL:
		return SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
	case D3DCMP_GREATER:
		return SDL_GPU_COMPAREOP_GREATER;
	case D3DCMP_NOTEQUAL:
		return SDL_GPU_COMPAREOP_NOT_EQUAL;
	case D3DCMP_GREATEREQUAL:
		return SDL_GPU_COMPAREOP_GREATER_OR_EQUAL;
	case D3DCMP_ALWAYS:
	default:
		return SDL_GPU_COMPAREOP_ALWAYS;
	}
}

// The subset of MiniwinRasterState that becomes pipeline-static, packed into a cache key.
Uint32 PipelineKey(const MiniwinRasterState& p_state)
{
	Uint32 key = 0;
	key |= (p_state.alphaBlend ? 1u : 0u) << 0;
	key |= ((Uint32) p_state.srcBlend & 0xf) << 1;
	key |= ((Uint32) p_state.destBlend & 0xf) << 5;
	key |= (p_state.zEnable ? 1u : 0u) << 9;
	key |= (p_state.zWrite ? 1u : 0u) << 10;
	key |= ((Uint32) p_state.zFunc & 0xf) << 11;
	key |= ((Uint32) p_state.cullMode & 0x3) << 15;
	return key;
}

// Selects the shader blob matching the device's supported formats. Returns false if
// none is available (an empty format, e.g. DXIL not generated on this host).
bool SelectShader(
	SDL_GPUShaderFormat p_deviceFormats,
	const unsigned char* p_spirv,
	unsigned int p_spirvLen,
	const unsigned char* p_msl,
	unsigned int p_mslLen,
	const unsigned char* p_dxil,
	unsigned int p_dxilLen,
	SDL_GPUShaderCreateInfo* p_info
)
{
	if ((p_deviceFormats & SDL_GPU_SHADERFORMAT_MSL) && p_mslLen) {
		p_info->format = SDL_GPU_SHADERFORMAT_MSL;
		p_info->code = p_msl;
		p_info->code_size = p_mslLen;
		p_info->entrypoint = "main0";
		return true;
	}
	if ((p_deviceFormats & SDL_GPU_SHADERFORMAT_SPIRV) && p_spirvLen) {
		p_info->format = SDL_GPU_SHADERFORMAT_SPIRV;
		p_info->code = p_spirv;
		p_info->code_size = p_spirvLen;
		p_info->entrypoint = "main";
		return true;
	}
	if ((p_deviceFormats & SDL_GPU_SHADERFORMAT_DXIL) && p_dxilLen) {
		p_info->format = SDL_GPU_SHADERFORMAT_DXIL;
		p_info->code = p_dxil;
		p_info->code_size = p_dxilLen;
		p_info->entrypoint = "main";
		return true;
	}
	return false;
}

// SDL surface pixel format whose memory byte order matches a GPU texture format, so a
// raw download reads back with correct channels (Metal swapchains are usually BGRA).
SDL_PixelFormat SurfaceFormatFor(SDL_GPUTextureFormat p_format)
{
	switch (p_format) {
	case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
		return SDL_PIXELFORMAT_ARGB8888;
	case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
		return SDL_PIXELFORMAT_ABGR8888;
	default:
		return SDL_PIXELFORMAT_ABGR8888;
	}
}

// The shader formats we ship blobs for (drives device creation so SDL never picks a
// driver whose format we lack).
SDL_GPUShaderFormat AvailableShaderFormats()
{
	SDL_GPUShaderFormat formats = 0;
	if (g_scene_vert_spirv_len) {
		formats |= SDL_GPU_SHADERFORMAT_SPIRV;
	}
	if (g_scene_vert_msl_len) {
		formats |= SDL_GPU_SHADERFORMAT_MSL;
	}
	if (g_scene_vert_dxil_len) {
		formats |= SDL_GPU_SHADERFORMAT_DXIL;
	}
	return formats;
}

class MiniwinSdlGpuBackend : public MiniwinRenderBackend {
public:
	~MiniwinSdlGpuBackend() override;

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
	SDL_GPUShader* LoadShader(SDL_GPUShaderStage p_stage);
	SDL_GPUGraphicsPipeline* PipelineFor(const MiniwinRasterState& p_state);
	SDL_GPUTexture* TextureFor(Uint32 p_id) const;
	void UploadPixels(SDL_GPUTexture* p_texture, int p_width, int p_height, const void* p_rgba, bool p_mipmaps);
	SDL_GPUTexture* CreateSolidTexture(Uint32 p_rgba);
	void EnsureSceneTargets();
	void UpdateViewport();
	void DrawableSize(int& p_width, int& p_height) const;
	void UploadStaging();
	void ReplayCommands();

	SDL_Window* m_window = nullptr;
	SDL_GPUDevice* m_device = nullptr;
	SDL_GPUShader* m_vertexShader = nullptr;
	SDL_GPUShader* m_fragmentShader = nullptr;
	SDL_GPUTextureFormat m_colorFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
	SDL_GPUTextureFormat m_depthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

	SDL_GPUTexture* m_sceneColor = nullptr;
	SDL_GPUTexture* m_sceneDepth = nullptr;
	int m_sceneW = 0;
	int m_sceneH = 0;

	// Letterboxed drawable-space rect the scene is blitted into.
	int m_vpX = 0;
	int m_vpY = 0;
	int m_vpW = 0;
	int m_vpH = 0;

	// The real drawable size, taken from the acquired swapchain — authoritative for the
	// viewport and scene target. SDL_GetWindowSizeInPixels can report the created window
	// size (e.g. 640x480) even when the swapchain is fullscreen (e.g. 1920x1080).
	int m_drawableW = 0;
	int m_drawableH = 0;

	std::unordered_map<Uint32, SDL_GPUGraphicsPipeline*> m_pipelines;
	std::unordered_map<Uint32, SDL_GPUTexture*> m_textures;
	Uint32 m_nextTextureId = 1;
	SDL_GPUSampler* m_samplers[2][2] = {}; // [linear][wrap]
	SDL_GPUTexture* m_dummyTexture = nullptr;

	SDL_GPUTransferBuffer* m_uploadBuffer = nullptr;
	Uint32 m_uploadCapacity = 0;

	SDL_GPUBuffer* m_vertexBuffer = nullptr;
	Uint32 m_vertexCapacity = 0;
	SDL_GPUBuffer* m_indexBuffer = nullptr;
	Uint32 m_indexCapacity = 0;

	std::vector<D3DTLVERTEX> m_vertexStaging;
	std::vector<Uint16> m_indexStaging;
	std::vector<GpuCommand> m_commands;

	// Present-time render state.
	SDL_GPUCommandBuffer* m_cmdbuf = nullptr;
	SDL_GPURenderPass* m_pass = nullptr;
	Uint64 m_frameCounter = 0;
};

MiniwinSdlGpuBackend::~MiniwinSdlGpuBackend()
{
	if (!m_device) {
		return;
	}
	for (auto& entry : m_pipelines) {
		SDL_ReleaseGPUGraphicsPipeline(m_device, entry.second);
	}
	for (auto& entry : m_textures) {
		SDL_ReleaseGPUTexture(m_device, entry.second);
	}
	for (auto& row : m_samplers) {
		for (SDL_GPUSampler* sampler : row) {
			if (sampler) {
				SDL_ReleaseGPUSampler(m_device, sampler);
			}
		}
	}
	if (m_dummyTexture) {
		SDL_ReleaseGPUTexture(m_device, m_dummyTexture);
	}
	if (m_sceneColor) {
		SDL_ReleaseGPUTexture(m_device, m_sceneColor);
	}
	if (m_sceneDepth) {
		SDL_ReleaseGPUTexture(m_device, m_sceneDepth);
	}
	if (m_uploadBuffer) {
		SDL_ReleaseGPUTransferBuffer(m_device, m_uploadBuffer);
	}
	if (m_vertexBuffer) {
		SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
	}
	if (m_indexBuffer) {
		SDL_ReleaseGPUBuffer(m_device, m_indexBuffer);
	}
	if (m_vertexShader) {
		SDL_ReleaseGPUShader(m_device, m_vertexShader);
	}
	if (m_fragmentShader) {
		SDL_ReleaseGPUShader(m_device, m_fragmentShader);
	}
	SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
	SDL_DestroyGPUDevice(m_device);
}

SDL_GPUShader* MiniwinSdlGpuBackend::LoadShader(SDL_GPUShaderStage p_stage)
{
	SDL_GPUShaderCreateInfo info = {};
	info.stage = p_stage;
	bool ok;
	if (p_stage == SDL_GPU_SHADERSTAGE_VERTEX) {
		info.num_uniform_buffers = 1;
		info.num_samplers = 0;
		ok = SelectShader(
			SDL_GetGPUShaderFormats(m_device),
			g_scene_vert_spirv,
			g_scene_vert_spirv_len,
			g_scene_vert_msl,
			g_scene_vert_msl_len,
			g_scene_vert_dxil,
			g_scene_vert_dxil_len,
			&info
		);
	}
	else {
		info.num_uniform_buffers = 1;
		info.num_samplers = 1;
		ok = SelectShader(
			SDL_GetGPUShaderFormats(m_device),
			g_scene_frag_spirv,
			g_scene_frag_spirv_len,
			g_scene_frag_msl,
			g_scene_frag_msl_len,
			g_scene_frag_dxil,
			g_scene_frag_dxil_len,
			&info
		);
	}
	if (!ok) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "no shader blob for the device's format");
		return nullptr;
	}
	SDL_GPUShader* shader = SDL_CreateGPUShader(m_device, &info);
	if (!shader) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUShader failed: %s", SDL_GetError());
	}
	return shader;
}

bool MiniwinSdlGpuBackend::Init(SDL_Window* p_window)
{
	m_window = p_window;

	// Create the device and claim the window on the main thread: on macOS the swapchain's
	// CAMetalLayer must be attached to the window's view there, or nothing composites even
	// though offscreen rendering works. Command submission afterwards is thread-safe.
	SDL_GPUDevice* device = nullptr;
	bool claimed = false;
	MiniwinApp_RunOnMainThread([&]() {
		device = SDL_CreateGPUDevice(AvailableShaderFormats(), false, nullptr);
		if (device) {
			claimed = SDL_ClaimWindowForGPUDevice(device, p_window);
		}
	});
	m_device = device;
	if (!m_device) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUDevice failed: %s", SDL_GetError());
		return false;
	}
	if (!claimed) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
		return false;
	}
	SDL_LogInfo(LOG_CATEGORY_MINIWIN, "SDL_GPU driver: %s", SDL_GetGPUDeviceDriver(m_device));

	m_colorFormat = SDL_GetGPUSwapchainTextureFormat(m_device, p_window);

	m_vertexShader = LoadShader(SDL_GPU_SHADERSTAGE_VERTEX);
	m_fragmentShader = LoadShader(SDL_GPU_SHADERSTAGE_FRAGMENT);
	if (!m_vertexShader || !m_fragmentShader) {
		return false;
	}

	for (int linear = 0; linear < 2; linear++) {
		for (int wrap = 0; wrap < 2; wrap++) {
			SDL_GPUSamplerCreateInfo info = {};
			info.min_filter = linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
			info.mag_filter = linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
			info.mipmap_mode = linear ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
			SDL_GPUSamplerAddressMode address =
				wrap ? SDL_GPU_SAMPLERADDRESSMODE_REPEAT : SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
			info.address_mode_u = address;
			info.address_mode_v = address;
			info.address_mode_w = address;
			m_samplers[linear][wrap] = SDL_CreateGPUSampler(m_device, &info);
		}
	}

	m_dummyTexture = CreateSolidTexture(0xFFFFFFFF);

	UpdateViewport();
	return true;
}

// Small helper kept out-of-line to avoid clutter in Init.
SDL_GPUTexture* CreateSceneTexture(
	SDL_GPUDevice* p_device,
	SDL_GPUTextureFormat p_format,
	int p_w,
	int p_h,
	SDL_GPUTextureUsageFlags p_usage
)
{
	SDL_GPUTextureCreateInfo info = {};
	info.type = SDL_GPU_TEXTURETYPE_2D;
	info.format = p_format;
	info.usage = p_usage;
	info.width = (Uint32) p_w;
	info.height = (Uint32) p_h;
	info.layer_count_or_depth = 1;
	info.num_levels = 1;
	info.sample_count = SDL_GPU_SAMPLECOUNT_1;
	return SDL_CreateGPUTexture(p_device, &info);
}

void MiniwinSdlGpuBackend::EnsureSceneTargets()
{
	int width;
	int height;
	if (MiniwinGetRenderResolution() == MINIWIN_RESOLUTION_ORIGINAL) {
		width = m_width;
		height = m_height;
	}
	else {
		DrawableSize(width, height);
	}
	if (width <= 0 || height <= 0) {
		width = m_width;
		height = m_height;
	}
	if (m_sceneColor && m_sceneW == width && m_sceneH == height) {
		return;
	}

	if (m_sceneColor) {
		SDL_ReleaseGPUTexture(m_device, m_sceneColor);
	}
	if (m_sceneDepth) {
		SDL_ReleaseGPUTexture(m_device, m_sceneDepth);
	}
	m_sceneColor = CreateSceneTexture(
		m_device,
		m_colorFormat,
		width,
		height,
		SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER
	);
	m_sceneDepth =
		CreateSceneTexture(m_device, m_depthFormat, width, height, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
	m_sceneW = width;
	m_sceneH = height;
}

void MiniwinSdlGpuBackend::DrawableSize(int& p_width, int& p_height) const
{
	if (m_drawableW > 0 && m_drawableH > 0) {
		p_width = m_drawableW;
		p_height = m_drawableH;
		return;
	}
	SDL_GetWindowSizeInPixels(m_window, &p_width, &p_height);
}

void MiniwinSdlGpuBackend::UpdateViewport()
{
	int dw = 0;
	int dh = 0;
	DrawableSize(dw, dh);
	if (dw <= 0 || dh <= 0 || m_width <= 0 || m_height <= 0) {
		return;
	}

	int vpW = dw;
	int vpH = dh;
	if (MiniwinGetScaleMode() == MINIWIN_SCALE_LETTERBOX) {
		vpH = (int) ((float) dw * (float) m_height / (float) m_width);
		if (vpH > dh) {
			vpH = dh;
			vpW = (int) ((float) dh * (float) m_width / (float) m_height);
		}
	}
	m_vpX = (dw - vpW) / 2;
	m_vpY = (dh - vpH) / 2;
	m_vpW = vpW;
	m_vpH = vpH;

	EnsureSceneTargets();
}

void MiniwinSdlGpuBackend::Resize(int p_width, int p_height)
{
	m_width = p_width;
	m_height = p_height;
	UpdateViewport();
}

SDL_GPUTexture* MiniwinSdlGpuBackend::TextureFor(Uint32 p_id) const
{
	auto found = m_textures.find(p_id);
	return found == m_textures.end() ? nullptr : found->second;
}

SDL_GPUGraphicsPipeline* MiniwinSdlGpuBackend::PipelineFor(const MiniwinRasterState& p_state)
{
	Uint32 key = PipelineKey(p_state);
	auto found = m_pipelines.find(key);
	if (found != m_pipelines.end()) {
		return found->second;
	}

	SDL_GPUVertexBufferDescription bufferDesc = {};
	bufferDesc.slot = 0;
	bufferDesc.pitch = sizeof(D3DTLVERTEX);
	bufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

	SDL_GPUVertexAttribute attributes[4] = {};
	attributes[0].location = 0;
	attributes[0].buffer_slot = 0;
	attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[0].offset = offsetof(D3DTLVERTEX, sx);
	attributes[1].location = 1;
	attributes[1].buffer_slot = 0;
	attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
	attributes[1].offset = offsetof(D3DTLVERTEX, color);
	attributes[2].location = 2;
	attributes[2].buffer_slot = 0;
	attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
	attributes[2].offset = offsetof(D3DTLVERTEX, specular);
	attributes[3].location = 3;
	attributes[3].buffer_slot = 0;
	attributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	attributes[3].offset = offsetof(D3DTLVERTEX, tu);

	SDL_GPUColorTargetBlendState blend = {};
	blend.enable_blend = p_state.alphaBlend;
	blend.src_color_blendfactor = BlendFactor(p_state.srcBlend);
	blend.dst_color_blendfactor = BlendFactor(p_state.destBlend);
	blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
	blend.src_alpha_blendfactor = BlendFactor(p_state.srcBlend);
	blend.dst_alpha_blendfactor = BlendFactor(p_state.destBlend);
	blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
	blend.color_write_mask =
		SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

	SDL_GPUColorTargetDescription colorTarget = {};
	colorTarget.format = m_colorFormat;
	colorTarget.blend_state = blend;

	SDL_GPUGraphicsPipelineCreateInfo info = {};
	info.vertex_shader = m_vertexShader;
	info.fragment_shader = m_fragmentShader;
	info.vertex_input_state.vertex_buffer_descriptions = &bufferDesc;
	info.vertex_input_state.num_vertex_buffers = 1;
	info.vertex_input_state.vertex_attributes = attributes;
	info.vertex_input_state.num_vertex_attributes = 4;
	info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	// Match the GL backend: cull back faces, front face selected by cull mode (the
	// projection's Y flip reverses the winding the rasterizer sees).
	if (p_state.cullMode == D3DCULL_NONE) {
		info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
	}
	else {
		info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
	}
	info.rasterizer_state.front_face =
		p_state.cullMode == D3DCULL_CW ? SDL_GPU_FRONTFACE_CLOCKWISE : SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
	info.depth_stencil_state.enable_depth_test = p_state.zEnable;
	info.depth_stencil_state.enable_depth_write = p_state.zWrite;
	info.depth_stencil_state.compare_op = CompareOp(p_state.zFunc);
	info.target_info.color_target_descriptions = &colorTarget;
	info.target_info.num_color_targets = 1;
	info.target_info.depth_stencil_format = m_depthFormat;
	info.target_info.has_depth_stencil_target = true;

	SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(m_device, &info);
	if (!pipeline) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUGraphicsPipeline failed: %s", SDL_GetError());
	}
	m_pipelines[key] = pipeline;
	return pipeline;
}

// --- Textures ---

Uint32 MiniwinSdlGpuBackend::CreateTexture(int p_width, int p_height, const void* p_rgba, bool p_mipmaps)
{
	SDL_GPUTextureCreateInfo info = {};
	info.type = SDL_GPU_TEXTURETYPE_2D;
	info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
	info.width = (Uint32) p_width;
	info.height = (Uint32) p_height;
	info.layer_count_or_depth = 1;
	info.num_levels = 1;
	info.sample_count = SDL_GPU_SAMPLECOUNT_1;
	if (p_mipmaps) {
		int maxDim = SDL_max(p_width, p_height);
		int levels = 1;
		while ((maxDim >>= 1) > 0) {
			levels++;
		}
		info.num_levels = (Uint32) levels;
		info.usage |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
	}

	SDL_GPUTexture* texture = SDL_CreateGPUTexture(m_device, &info);
	if (!texture) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_CreateGPUTexture failed: %s", SDL_GetError());
		return 0;
	}

	Uint32 id = m_nextTextureId++;
	m_textures[id] = texture;
	UpdateTexture(id, p_width, p_height, p_rgba, p_mipmaps);
	return id;
}

void MiniwinSdlGpuBackend::UploadPixels(
	SDL_GPUTexture* p_texture,
	int p_width,
	int p_height,
	const void* p_rgba,
	bool p_mipmaps
)
{
	if (!p_texture || !p_rgba) {
		return;
	}

	Uint32 byteSize = (Uint32) p_width * (Uint32) p_height * 4;
	SDL_GPUTransferBufferCreateInfo transferInfo = {};
	transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transferInfo.size = byteSize;
	SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(m_device, &transferInfo);
	if (!transfer) {
		return;
	}
	void* mapped = SDL_MapGPUTransferBuffer(m_device, transfer, false);
	SDL_memcpy(mapped, p_rgba, byteSize);
	SDL_UnmapGPUTransferBuffer(m_device, transfer);

	SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_device);
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);
	SDL_GPUTextureTransferInfo source = {};
	source.transfer_buffer = transfer;
	source.pixels_per_row = (Uint32) p_width;
	source.rows_per_layer = (Uint32) p_height;
	SDL_GPUTextureRegion region = {};
	region.texture = p_texture;
	region.w = (Uint32) p_width;
	region.h = (Uint32) p_height;
	region.d = 1;
	SDL_UploadToGPUTexture(copyPass, &source, &region, false);
	SDL_EndGPUCopyPass(copyPass);
	if (p_mipmaps) {
		SDL_GenerateMipmapsForGPUTexture(cmd, p_texture);
	}
	SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
	if (fence) {
		SDL_WaitForGPUFences(m_device, true, &fence, 1);
		SDL_ReleaseGPUFence(m_device, fence);
	}
	SDL_ReleaseGPUTransferBuffer(m_device, transfer);
}

void MiniwinSdlGpuBackend::UpdateTexture(Uint32 p_id, int p_width, int p_height, const void* p_rgba, bool p_mipmaps)
{
	UploadPixels(TextureFor(p_id), p_width, p_height, p_rgba, p_mipmaps);
}

SDL_GPUTexture* MiniwinSdlGpuBackend::CreateSolidTexture(Uint32 p_rgba)
{
	SDL_GPUTextureCreateInfo info = {};
	info.type = SDL_GPU_TEXTURETYPE_2D;
	info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
	info.width = 1;
	info.height = 1;
	info.layer_count_or_depth = 1;
	info.num_levels = 1;
	info.sample_count = SDL_GPU_SAMPLECOUNT_1;
	SDL_GPUTexture* texture = SDL_CreateGPUTexture(m_device, &info);
	UploadPixels(texture, 1, 1, &p_rgba, false);
	return texture;
}

void MiniwinSdlGpuBackend::DestroyTexture(Uint32 p_id)
{
	auto found = m_textures.find(p_id);
	if (found != m_textures.end()) {
		SDL_ReleaseGPUTexture(m_device, found->second);
		m_textures.erase(found);
	}
}

// --- Frame ---

void MiniwinSdlGpuBackend::BeginScene()
{
}

void MiniwinSdlGpuBackend::EndScene()
{
}

void MiniwinSdlGpuBackend::Clear(const SDL_Rect*, float p_r, float p_g, float p_b, bool p_color, bool p_depth)
{
	GpuCommand command = {};
	command.m_kind = GpuCommand::Clear;
	command.m_clearColor = p_color;
	command.m_clearDepth = p_depth;
	command.m_r = p_r;
	command.m_g = p_g;
	command.m_b = p_b;
	m_commands.push_back(command);
}

void MiniwinSdlGpuBackend::DrawTriangles(
	const MiniwinRasterState& p_state,
	const D3DTLVERTEX* p_vertices,
	Uint32 p_vertexCount,
	const Uint16* p_indices,
	Uint32 p_indexCount
)
{
	GpuCommand command = {};
	command.m_kind = GpuCommand::Draw;
	command.m_state = p_state;
	command.m_vertexOffset = (Uint32) m_vertexStaging.size();
	command.m_vertexCount = p_vertexCount;
	m_vertexStaging.insert(m_vertexStaging.end(), p_vertices, p_vertices + p_vertexCount);
	if (p_indices) {
		command.m_indexed = true;
		command.m_indexOffset = (Uint32) m_indexStaging.size();
		command.m_indexCount = p_indexCount;
		m_indexStaging.insert(m_indexStaging.end(), p_indices, p_indices + p_indexCount);
	}
	m_commands.push_back(command);
}

void MiniwinSdlGpuBackend::UploadStaging()
{
	Uint32 vertexBytes = (Uint32) (m_vertexStaging.size() * sizeof(D3DTLVERTEX));
	Uint32 indexBytes = (Uint32) (m_indexStaging.size() * sizeof(Uint16));
	if (vertexBytes == 0) {
		return;
	}

	if (vertexBytes > m_vertexCapacity) {
		if (m_vertexBuffer) {
			SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
		}
		SDL_GPUBufferCreateInfo info = {};
		info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
		info.size = vertexBytes;
		m_vertexBuffer = SDL_CreateGPUBuffer(m_device, &info);
		m_vertexCapacity = vertexBytes;
	}
	if (indexBytes > m_indexCapacity) {
		if (m_indexBuffer) {
			SDL_ReleaseGPUBuffer(m_device, m_indexBuffer);
		}
		SDL_GPUBufferCreateInfo info = {};
		info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
		info.size = indexBytes;
		m_indexBuffer = SDL_CreateGPUBuffer(m_device, &info);
		m_indexCapacity = indexBytes;
	}

	Uint32 total = vertexBytes + indexBytes;
	if (total > m_uploadCapacity) {
		if (m_uploadBuffer) {
			SDL_ReleaseGPUTransferBuffer(m_device, m_uploadBuffer);
		}
		SDL_GPUTransferBufferCreateInfo info = {};
		info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		info.size = total;
		m_uploadBuffer = SDL_CreateGPUTransferBuffer(m_device, &info);
		m_uploadCapacity = total;
	}

	Uint8* mapped = (Uint8*) SDL_MapGPUTransferBuffer(m_device, m_uploadBuffer, true);
	SDL_memcpy(mapped, m_vertexStaging.data(), vertexBytes);
	if (indexBytes) {
		SDL_memcpy(mapped + vertexBytes, m_indexStaging.data(), indexBytes);
	}
	SDL_UnmapGPUTransferBuffer(m_device, m_uploadBuffer);

	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(m_cmdbuf);
	SDL_GPUTransferBufferLocation source = {};
	source.transfer_buffer = m_uploadBuffer;
	source.offset = 0;
	SDL_GPUBufferRegion vertexRegion = {};
	vertexRegion.buffer = m_vertexBuffer;
	vertexRegion.offset = 0;
	vertexRegion.size = vertexBytes;
	SDL_UploadToGPUBuffer(copyPass, &source, &vertexRegion, true);
	if (indexBytes) {
		source.offset = vertexBytes;
		SDL_GPUBufferRegion indexRegion = {};
		indexRegion.buffer = m_indexBuffer;
		indexRegion.offset = 0;
		indexRegion.size = indexBytes;
		SDL_UploadToGPUBuffer(copyPass, &source, &indexRegion, true);
	}
	SDL_EndGPUCopyPass(copyPass);
}

void MiniwinSdlGpuBackend::ReplayCommands()
{
	VertexUniforms vertexUniforms = {};
	vertexUniforms.m_viewport[0] = (float) m_width;
	vertexUniforms.m_viewport[1] = (float) m_height;

	bool passActive = false;
	auto beginPass = [&](bool p_clearColor, float p_r, float p_g, float p_b, bool p_clearDepth) {
		if (passActive) {
			SDL_EndGPURenderPass(m_pass);
			passActive = false;
		}
		SDL_GPUColorTargetInfo colorTarget = {};
		colorTarget.texture = m_sceneColor;
		colorTarget.load_op = p_clearColor ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
		colorTarget.store_op = SDL_GPU_STOREOP_STORE;
		colorTarget.clear_color = {p_r, p_g, p_b, 1.0f};
		SDL_GPUDepthStencilTargetInfo depthTarget = {};
		depthTarget.texture = m_sceneDepth;
		depthTarget.load_op = p_clearDepth ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
		depthTarget.store_op = SDL_GPU_STOREOP_STORE;
		depthTarget.clear_depth = 1.0f;
		m_pass = SDL_BeginGPURenderPass(m_cmdbuf, &colorTarget, 1, &depthTarget);
		passActive = true;

		SDL_GPUViewport viewport = {};
		viewport.w = (float) m_sceneW;
		viewport.h = (float) m_sceneH;
		viewport.min_depth = 0.0f;
		viewport.max_depth = 1.0f;
		SDL_SetGPUViewport(m_pass, &viewport);
		SDL_PushGPUVertexUniformData(m_cmdbuf, 0, &vertexUniforms, sizeof(vertexUniforms));
	};

	for (const GpuCommand& command : m_commands) {
		if (command.m_kind == GpuCommand::Clear) {
			beginPass(command.m_clearColor, command.m_r, command.m_g, command.m_b, command.m_clearDepth);
			continue;
		}

		if (!passActive) {
			beginPass(false, 0.0f, 0.0f, 0.0f, false);
		}

		SDL_GPUGraphicsPipeline* pipeline = PipelineFor(command.m_state);
		if (!pipeline) {
			continue;
		}
		SDL_BindGPUGraphicsPipeline(m_pass, pipeline);

		FragmentUniforms fragmentUniforms = {};
		fragmentUniforms.m_colorOp = (Sint32) command.m_state.colorOp;
		fragmentUniforms.m_alphaOp = (Sint32) command.m_state.alphaOp;
		fragmentUniforms.m_specular = command.m_state.specular ? 1 : 0;
		fragmentUniforms.m_alphaFunc = command.m_state.alphaTest ? (Sint32) command.m_state.alphaFunc : 0;
		fragmentUniforms.m_alphaRef = command.m_state.alphaRef;
		fragmentUniforms.m_colorKey = command.m_state.colorKeyTest ? 1 : 0;
		SDL_PushGPUFragmentUniformData(m_cmdbuf, 0, &fragmentUniforms, sizeof(fragmentUniforms));

		SDL_GPUTexture* texture = command.m_state.textured ? TextureFor(command.m_state.textureId) : nullptr;
		if (!texture) {
			texture = m_dummyTexture;
		}
		SDL_GPUTextureSamplerBinding samplerBinding = {};
		samplerBinding.texture = texture;
		samplerBinding.sampler = m_samplers[command.m_state.textureLinear ? 1 : 0][command.m_state.textureWrap ? 1 : 0];
		SDL_BindGPUFragmentSamplers(m_pass, 0, &samplerBinding, 1);

		SDL_GPUBufferBinding vertexBinding = {};
		vertexBinding.buffer = m_vertexBuffer;
		vertexBinding.offset = 0;
		SDL_BindGPUVertexBuffers(m_pass, 0, &vertexBinding, 1);

		if (command.m_indexed) {
			SDL_GPUBufferBinding indexBinding = {};
			indexBinding.buffer = m_indexBuffer;
			indexBinding.offset = 0;
			SDL_BindGPUIndexBuffer(m_pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
			SDL_DrawGPUIndexedPrimitives(
				m_pass,
				command.m_indexCount,
				1,
				command.m_indexOffset,
				(Sint32) command.m_vertexOffset,
				0
			);
		}
		else {
			SDL_DrawGPUPrimitives(m_pass, command.m_vertexCount, 1, command.m_vertexOffset, 0);
		}
	}

	if (passActive) {
		SDL_EndGPURenderPass(m_pass);
	}
	m_pass = nullptr;
}

void MiniwinSdlGpuBackend::Present()
{
	MiniwinPhaseScope phase(MINIWIN_PHASE_PRESENT);
	m_frameCounter++;

	m_cmdbuf = SDL_AcquireGPUCommandBuffer(m_device);
	if (!m_cmdbuf) {
		m_commands.clear();
		m_vertexStaging.clear();
		m_indexStaging.clear();
		return;
	}

	// Acquire the swapchain first so the viewport and scene target are sized to the real
	// drawable (its dimensions are authoritative — see DrawableSize).
	SDL_GPUTexture* swapchain = nullptr;
	Uint32 swapchainW = 0;
	Uint32 swapchainH = 0;
	if (SDL_WaitAndAcquireGPUSwapchainTexture(m_cmdbuf, m_window, &swapchain, &swapchainW, &swapchainH) &&
		swapchainW > 0 && swapchainH > 0) {
		m_drawableW = (int) swapchainW;
		m_drawableH = (int) swapchainH;
	}

	UpdateViewport();
	UploadStaging();
	ReplayCommands();

	if (swapchain) {
		// Letterbox the scene into the swapchain; clear the borders to black.
		SDL_GPUBlitInfo blit = {};
		blit.source.texture = m_sceneColor;
		blit.source.w = (Uint32) m_sceneW;
		blit.source.h = (Uint32) m_sceneH;
		blit.destination.texture = swapchain;
		blit.destination.x = (Uint32) m_vpX;
		blit.destination.y = (Uint32) m_vpY;
		blit.destination.w = (Uint32) m_vpW;
		blit.destination.h = (Uint32) m_vpH;
		blit.load_op = SDL_GPU_LOADOP_CLEAR;
		blit.clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
		blit.filter = SDL_GPU_FILTER_LINEAR;
		SDL_BlitGPUTexture(m_cmdbuf, &blit);
	}

	SDL_SubmitGPUCommandBuffer(m_cmdbuf);
	m_cmdbuf = nullptr;

	m_commands.clear();
	m_vertexStaging.clear();
	m_indexStaging.clear();

	MiniwinBackend_HandleFrameDump(this, m_frameCounter);
}

SDL_Surface* MiniwinSdlGpuBackend::ReadBackbuffer()
{
	Uint32 byteSize = (Uint32) m_sceneW * (Uint32) m_sceneH * 4;
	SDL_GPUTransferBufferCreateInfo info = {};
	info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
	info.size = byteSize;
	SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(m_device, &info);
	if (!transfer) {
		return nullptr;
	}

	SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_device);
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);
	SDL_GPUTextureRegion region = {};
	region.texture = m_sceneColor;
	region.w = (Uint32) m_sceneW;
	region.h = (Uint32) m_sceneH;
	region.d = 1;
	SDL_GPUTextureTransferInfo destination = {};
	destination.transfer_buffer = transfer;
	destination.pixels_per_row = (Uint32) m_sceneW;
	destination.rows_per_layer = (Uint32) m_sceneH;
	SDL_DownloadFromGPUTexture(copyPass, &region, &destination);
	SDL_EndGPUCopyPass(copyPass);
	SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
	if (fence) {
		SDL_WaitForGPUFences(m_device, true, &fence, 1);
		SDL_ReleaseGPUFence(m_device, fence);
	}

	SDL_Surface* raw = SDL_CreateSurface(m_sceneW, m_sceneH, SurfaceFormatFor(m_colorFormat));
	void* mapped = SDL_MapGPUTransferBuffer(m_device, transfer, false);
	if (raw && mapped) {
		SDL_memcpy(raw->pixels, mapped, byteSize);
	}
	SDL_UnmapGPUTransferBuffer(m_device, transfer);
	SDL_ReleaseGPUTransferBuffer(m_device, transfer);

	// Normalize to a canonical layout so screenshots match the OpenGL backend.
	SDL_Surface* surface = raw ? SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ABGR8888) : nullptr;
	SDL_DestroySurface(raw);
	return surface;
}

} // namespace

bool MiniwinSdlGpu_Available()
{
	SDL_GPUDevice* device = SDL_CreateGPUDevice(AvailableShaderFormats(), false, nullptr);
	if (!device) {
		return false;
	}
	SDL_DestroyGPUDevice(device);
	return true;
}

Uint32 MiniwinSdlGpu_PrepareWindowFlags()
{
	// SDL_GPU claims the window in Init; no window-creation flags are required.
	return 0;
}

MiniwinRenderBackend* MiniwinSdlGpu_Create(SDL_Window* p_window, int p_width, int p_height)
{
	MiniwinSdlGpuBackend* backend = new MiniwinSdlGpuBackend();
	backend->m_width = p_width;
	backend->m_height = p_height;
	if (!backend->Init(p_window)) {
		delete backend;
		return nullptr;
	}
	return backend;
}
