// Vulkan port of New3D's fragmentShaderFog (Src/Graphics/New3D/R3DScrollFog.cpp).
// Verbatim except #version and the five loose uniforms, which become one
// std140 block (R3DScrollFog.cpp's ScrollFogBlock mirrors it).

#version 450

layout(std140, set = 0, binding = 0) uniform ScrollFogBlock
{
	vec4	fogColour;
	vec4	spotEllipse;
	vec3	spotFogColor;
	float	fogAttenuation;
	float	fogAmbient;
};

// Spotlight on fog
float	ellipse;
vec2	position, size;
vec3	lSpotFogColor;

// Scroll fog
float	lfogAttenuation;
vec3	lFogColor;
vec4	scrollFog;

// outputs
layout(location = 0) out vec4 out0;		// opaque
layout(location = 1) out vec4 out1;		// trans layer 1
layout(location = 2) out vec4 out2;		// trans layer 2

void WriteOutputs(vec4 colour)
{
	vec4 blank = vec4(0.0);

	out0 = colour;
	out1 = blank;
	out2 = blank;	
}

void main()
{
	// Scroll fog base color
	lFogColor = fogColour.rgb * fogAmbient;

	// Spotlight on fog (area) 
	position = spotEllipse.xy;
	size = spotEllipse.zw;
	ellipse = length((gl_FragCoord.xy - position) / size);
	ellipse = ellipse * ellipse;			// decay rate = square of distance from center
	ellipse = 1.0 - ellipse;				// invert
	ellipse = max(0.0, ellipse);			// clamp

	// Spotlight on fog (color)
	lSpotFogColor = mix(vec3(0.0), spotFogColor * ellipse * fogColour.rgb, fogAttenuation);

	// Scroll fog density
	scrollFog = vec4(lFogColor + lSpotFogColor, fogColour.a);

	// Final Color
	WriteOutputs(scrollFog);
}
