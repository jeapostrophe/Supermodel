// The three std140 uniform blocks R3DShader drives, in ONE file so that the
// vertex and fragment stages cannot drift apart and so that the C++ mirror
// structs in R3DShader.h have a single source of truth to be checked against
// (see R3DShader.cpp's static_asserts and Shaders/R3DBlockOffsets.h, which
// Scripts/build-spirv.sh generates out of the compiled SPIR-V).
//
// These are New3D's loose `uniform`s, grouped by how often they change:
// per viewport, per model, per mesh. The blocks are unnamed, so every member
// keeps the name the GL shaders used and the shader bodies stay verbatim.
//
// The blocks are declared identically in both stages even where a stage uses
// only some of the members: the descriptor set layout is one layout, and a
// member that moves in one stage and not the other is the failure this file
// exists to prevent.

layout(std140, set = 0, binding = 0) uniform ViewportBlock
{
	mat4	projMat;
	vec3	lighting[2];		// lighting state (lighting[0] = sun direction, lighting[1].x,y = diffuse, ambient intensities from 0-1.0)
	vec4	spotEllipse;		// spotlight ellipse position: .x=X position (screen coordinates), .y=Y position, .z=half-width, .w=half-height)
	vec3	fogColour;
	vec3	spotColor;			// spotlight RGB color
	vec3	spotFogColor;		// spotlight RGB color on fog
	vec2	spotRange;			// spotlight Z range: .x=start (viewspace coordinates), .y=limit
	float	fogDensity;
	float	fogStart;
	float	fogAttenuation;
	float	fogAmbient;
	float	cota;
	int		hardwareStep;
	bool	sunClamp;			// not used by daytona and la machine guns
	bool	intensityClamp;		// some games such as daytona and
};

layout(std140, set = 0, binding = 1) uniform ModelBlock
{
	mat4	modelMat;
	float	modelScale;
	float	nodeAlpha;
};

layout(std140, set = 0, binding = 2) uniform MeshBlock
{
	ivec4	baseTexInfo;		// x/y are x,y positions in the texture sheet. z/w are with and height
	ivec2	textureWrapMode;
	int		texturePage;
	int		baseTexType;
	int		microTextureID;
	int		colourLayer;
	float	microTextureMinLOD;
	float	fogIntensity;
	float	shininess;			// specular shininess
	float	specularValue;		// specular coefficient
	bool	textureEnabled;
	bool	microTexture;
	bool	textureInverted;
	bool	textureAlpha;
	bool	alphaTest;
	bool	discardAlpha;
	bool	lightEnabled;		// lighting enabled (1.0) or luminous (0.0), drawn at full intensity
	bool	specularEnabled;	// specular enabled
	bool	fixedShading;
	bool	smoothShading;
	bool	translatorMap;
	bool	polyAlpha;
};
