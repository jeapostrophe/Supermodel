// Vulkan port of R3DFrameBuffers::AllocShaderTrans's vertex shader
// (Src/Graphics/New3D/R3DFrameBuffers.cpp). Verbatim except #version and
// gl_VertexID -> gl_VertexIndex.

#version 450

void main(void)
{
	const vec4 vertices[] = vec4[](vec4(-1.0, -1.0, 0.0, 1.0),
									vec4(-1.0,  1.0, 0.0, 1.0),
									vec4( 1.0, -1.0, 0.0, 1.0),
									vec4( 1.0,  1.0, 0.0, 1.0));

	gl_Position = vertices[gl_VertexIndex % 4];
}
