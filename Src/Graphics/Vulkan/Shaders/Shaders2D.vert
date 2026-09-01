// The Vulkan port of Src/Graphics/Shaders2D.h's s_vertexShader.
// Verbatim except for the mechanical Vulkan edits: #version 450,
// gl_VertexID -> gl_VertexIndex, and an explicit location on the varying.
#version 450

// outputs
layout(location = 0) out vec2 fsTexCoord;

void main(void)
{
	const vec4 vertices[] = vec4[](vec4(-1.0, -1.0, 0.0, 1.0),
									vec4(-1.0,  1.0, 0.0, 1.0),
									vec4( 1.0, -1.0, 0.0, 1.0),
									vec4( 1.0,  1.0, 0.0, 1.0));

	const vec2 texCoords[] = vec2[](vec2(0.0, 1.0), // flip upside down
									vec2(0.0, 0.0),
									vec2(1.0, 1.0),
									vec2(1.0, 0.0));

	fsTexCoord = texCoords[gl_VertexIndex % 4];
	gl_Position = vertices[gl_VertexIndex % 4];
}
