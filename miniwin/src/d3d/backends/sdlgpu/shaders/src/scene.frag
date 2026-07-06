#version 450

// Fixed-function texture-stage emulation. Ported from the OpenGL 3.3 backend's fragment
// shader; SDL_GPU descriptor-set conventions (fragment samplers in set 2, fragment
// uniform buffers in set 3). The loose GL uniforms become one uniform block.

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec4 vSpecular;
layout(location = 2) in vec2 vUV;

layout(set = 2, binding = 0) uniform sampler2D uTexture;

layout(set = 3, binding = 0) uniform FragmentUniforms {
	int uColorOp;   // 0 = texture * diffuse, 1 = texture, 2 = diffuse
	int uAlphaOp;   // same encoding for the alpha channel
	int uSpecular;  // D3DRENDERSTATE_SPECULARENABLE
	int uAlphaFunc; // D3DCMPFUNC or 0 = disabled
	float uAlphaRef;
	int uColorKey;  // discard fragments whose sampled texture alpha is keyed out
	vec2 uPad;
};

layout(location = 0) out vec4 oColor;

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
