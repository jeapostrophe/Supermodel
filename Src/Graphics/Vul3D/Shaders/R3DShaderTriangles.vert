// Vulkan port of New3D's vertexShaderR3D (Src/Graphics/New3D/R3DShaderTriangles.h).
// The body is verbatim; only the declarations changed: GLSL 450, the loose
// uniforms moved into R3DBlocks.glsl, and explicit locations on every input,
// attribute and varying.

#version 450

// uniforms
#include "R3DBlocks.glsl"

// attributes
layout(location = 0) in vec4	inVertex;
layout(location = 1) in vec3	inNormal;
layout(location = 2) in vec2	inTexCoord;
layout(location = 3) in vec4	inColour;
layout(location = 4) in vec3	inFaceNormal;		// used to emulate r3d culling
layout(location = 5) in float	inFixedShade;
layout(location = 6) in float	inTextureNP;

// outputs to fragment shader
layout(location = 0) out vec3	fsViewVertex;
layout(location = 1) out vec3	fsViewNormal;		// per vertex normal vector
layout(location = 2) out vec2	fsTexCoord;
layout(location = 3) out vec4	fsColor;
layout(location = 4) out float	fsFixedShade;
layout(location = 5) out float	fsDiscardPoly;		// can't have varying bool (glsl spec)
layout(location = 6) out float	fsLODBase;

vec4 GetColour(vec4 colour)
{
	vec4 c = colour;

	if(translatorMap) {
		c.rgb *= 16.0;
	}

	c.a *= nodeAlpha;

	return c;
}

float CalcBackFace(in vec3 viewVertex)
{
	vec3 vt = viewVertex; // - vec3(0.0);
	vec3 vn = mat3(modelMat) * inFaceNormal;

	// dot product of face normal with view direction
	return dot(vt, vn);
}

void main(void)
{
	fsViewVertex	= (modelMat * inVertex).xyz;
	fsViewNormal	= (mat3(modelMat) / modelScale) * inNormal;
	fsDiscardPoly	= CalcBackFace(fsViewVertex);
	fsColor			= GetColour(inColour);
	fsTexCoord		= inTexCoord;
	fsFixedShade	= inFixedShade;
	fsLODBase		= fsDiscardPoly * -cota * inTextureNP;
	gl_Position		= (projMat * modelMat) * inVertex;
}
