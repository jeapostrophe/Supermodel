// The Vulkan port of SuperAA::SuperAA's vertexShader (Src/Graphics/SuperAA.cpp).
// Verbatim except for #version 450, gl_VertexID -> gl_VertexIndex and an
// explicit location on the varying.
#version 450

layout(location = 0) out vec2 texCoord;

void main(void)
{
	const vec4 vertices[] = vec4[](vec4(-1.0, -1.0, 0.0, 1.0),
									vec4(-1.0,  1.0, 0.0, 1.0),
									vec4( 1.0, -1.0, 0.0, 1.0),
									vec4( 1.0,  1.0, 0.0, 1.0));
	const vec2 texCoords[] = vec2[](vec2(0.0, 0.0),
									vec2(0.0, 1.0),
									vec2(1.0, 0.0),
									vec2(1.0, 1.0));

	gl_Position = vertices[gl_VertexIndex % 4];
	texCoord = texCoords[gl_VertexIndex % 4];
}
