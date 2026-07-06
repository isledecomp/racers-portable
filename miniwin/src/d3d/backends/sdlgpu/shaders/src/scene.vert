#version 450

// D3DTLVERTEX: pre-transformed screen-space vertices. Ported from the OpenGL 3.3
// backend's vertex shader; SDL_GPU descriptor-set conventions (vertex uniform buffers
// live in set 1). The diffuse/specular attributes arrive as UBYTE4_NORM in D3DCOLOR
// (BGRA) byte order, so swizzle .bgra to RGBA — the GL backend got this from GL_BGRA.

layout(location = 0) in vec4 aPos;      // sx, sy, sz, rhw
layout(location = 1) in vec4 aColor;    // D3DCOLOR diffuse (BGRA)
layout(location = 2) in vec4 aSpecular; // D3DCOLOR specular (BGRA)
layout(location = 3) in vec2 aUV;

layout(set = 1, binding = 0) uniform VertexUniforms {
	vec2 uViewport; // logical render size (e.g. 640x480)
	vec2 uPad;
};

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec4 vSpecular;
layout(location = 2) out vec2 vUV;

void main()
{
	// D3D TL vertices are in screen space with rhw = 1/w. Reconstruct clip space so the
	// hardware perspective-corrects colors/UVs exactly like Direct3D 6 did.
	float w = 1.0 / aPos.w;
	float x = (aPos.x * 2.0 / uViewport.x - 1.0) * w;
	float y = (1.0 - aPos.y * 2.0 / uViewport.y) * w;
	float z = (aPos.z * 2.0 - 1.0) * w;
	gl_Position = vec4(x, y, z, w);
	vColor = aColor.bgra;
	vSpecular = aSpecular.bgra;
	vUV = aUV;
}
