// Vulkan port of R3DFrameBuffers::AllocShaderBase's fragment shader
// (Src/Graphics/New3D/R3DFrameBuffers.cpp). Verbatim except #version and the
// sampler's set/binding.

#version 450

// inputs
layout(set = 0, binding = 0) uniform sampler2D tex1;			// base tex

// outputs
layout(location = 0) out vec4 fragColor;

void main()
{
	ivec2 tc = ivec2(gl_FragCoord.xy /*-vec2(0.5)*/);
	fragColor = texelFetch(tex1, tc, 0);
}
