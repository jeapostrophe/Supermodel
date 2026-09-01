// The Vulkan port of InitializePointerOverlay's vertex_source
// (Src/OSD/SDL/LibRetro.cpp). Verbatim except for #version 450, an explicit
// input location, and the two loose uniforms becoming one push-constant block
// (padded to vec4 so the block's std430 offsets are 0 and 16).
#version 450

layout(location = 0) in vec2 in_pos;

layout(push_constant) uniform PointerOverlayPush
{
	vec4 u_resolution;	// xy used
	vec4 u_color;
} push;

void main(void)
{
	vec2 zero_to_one = in_pos / push.u_resolution.xy;
	vec2 zero_to_two = zero_to_one * 2.0;
	vec2 clip = zero_to_two - 1.0;
	gl_Position = vec4(clip.x, -clip.y, 0.0, 1.0);
}
