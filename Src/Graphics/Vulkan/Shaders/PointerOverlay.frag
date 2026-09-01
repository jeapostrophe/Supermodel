// The Vulkan port of InitializePointerOverlay's fragment_source
// (Src/OSD/SDL/LibRetro.cpp). The push-constant block must match
// PointerOverlay.vert exactly.
#version 450

layout(push_constant) uniform PointerOverlayPush
{
	vec4 u_resolution;	// xy used
	vec4 u_color;
} push;

layout(location = 0) out vec4 frag_color;

void main(void)
{
	frag_color = push.u_color;
}
