// Vulkan port of New3D's fragmentShaderR3D (Src/Graphics/New3D/R3DShaderTriangles.h)
// plus fragmentShaderR3DCommon (R3DShaderCommon.h), which GL concatenated as a
// second source string and which is #included at the bottom here for the same
// reason: the forward declarations below are satisfied by that file.
//
// The body is verbatim; only the declarations changed: GLSL 450, the loose
// uniforms moved into R3DBlocks.glsl, the sampler given a set/binding, and
// explicit locations on every varying.

#version 450

// uniforms
#include "R3DBlocks.glsl"

layout(set = 0, binding = 3) uniform usampler2D textureBank[2];			// entire texture sheet

//interpolated inputs from vertex shader
layout(location = 0) in vec3	fsViewVertex;
layout(location = 1) in vec3	fsViewNormal;		// per vertex normal vector
layout(location = 2) in vec2	fsTexCoord;
layout(location = 3) in vec4	fsColor;
layout(location = 4) in float	fsFixedShade;
layout(location = 5) in float	fsDiscardPoly;
layout(location = 6) in float	fsLODBase;

//outputs
layout(location = 0) out vec4 out0;		// opaque
layout(location = 1) out vec4 out1;		// trans layer 1
layout(location = 2) out vec4 out2;		// trans layer 2

// forward declarations (see common file)

float CalcFog();
void Step15Luminous(inout vec4 colour);
vec4 GetTextureValue();
void WriteOutputs(vec4 colour, int layer);
float Sqr(float a);
float SqrLength(vec2 a);
void ComputeSpotlight(vec4 spotEllipse, vec2 spotRange, float viewZ, out float lobeEffect, out float lobeFogEffect);

void main()
{
	vec4 tex1Data;
	vec4 colData;
	vec4 finalData;
	vec4 fogData;

	if(fsDiscardPoly > 0.0) {
		discard;		//emulate back face culling here
	}

	gl_FragDepth = projMat[3][2] * gl_FragCoord.w;

	fogData = vec4(fogColour.rgb * fogAmbient, CalcFog());
	tex1Data = vec4(1.0, 1.0, 1.0, 1.0);

	if(textureEnabled) {
		tex1Data = GetTextureValue();
	}

	colData = fsColor;
	Step15Luminous(colData);			// no-op for step 2.0+
	finalData = tex1Data * colData;

	if (finalData.a < (1.0/32.0)) {		// basically chuck out any totally transparent pixels value = 1/16 the smallest transparency level h/w supports
		discard;
	}

	float lobeEffect;
	float lobeFogEffect;
	ComputeSpotlight(spotEllipse, spotRange, fsViewVertex.z, lobeEffect, lobeFogEffect);

	if (lightEnabled) {
		vec3   lightIntensity;
		vec3   sunVector;     // sun lighting vector (as reflecting away from vertex)
		float  sunFactor;     // sun light projection along vertex normal (0.0 to 1.0)

		// Sun angle
		sunVector = lighting[0];

		// Compute diffuse factor for sunlight
		if(fixedShading) {
			sunFactor = fsFixedShade;
		}
		else {
			sunFactor = dot(sunVector, fsViewNormal);
		}

		// Clamp ceil, fix for upscaled models without "modelScale" defined
		sunFactor = clamp(sunFactor,-1.0,1.0);

		// Optional clamping, value is allowed to be negative
		// We suspect that translucent polygons are always clamped (e.g. lasers in Daytona 2)
		if(sunClamp || finalData.a<0.999) {
			sunFactor = max(sunFactor,0.0);
		}

		// Total light intensity: sum of all components
		lightIntensity = vec3(sunFactor*lighting[1].x + lighting[1].y);   // diffuse + ambient

		lightIntensity += spotColor*lobeEffect;

		// Upper clamp is optional, step 1.5+ games will drive brightness beyond 100%
		if(intensityClamp) {
			lightIntensity = min(lightIntensity,1.0);
		}

		finalData.rgb *= lightIntensity;

		// for now assume fixed shading doesn't work with specular
		if (specularEnabled) {

			float specularFactor;

			if (smoothShading)
			{
				// Always clamp floor to zero
				float NdotL = max(0.0, sunFactor);

				const float expIndex[4]  = float[4](8.0, 16.0, 32.0, 64.0);
				const float multIndex[4] = float[4](1.6, 1.6, 2.4, 3.2);
				float exponent = expIndex[int(shininess)];

				specularFactor = pow(NdotL, exponent);
				specularFactor *= multIndex[int(shininess)];
			}
			else
			{
				// flat shaded polys use Phong reflection model (R dot V) without any exponent or multiplier
				// V = (0.0, 0.0, 1.0) is used by Model 3 as a fast approximation, so R dot V = R.z
				vec3 R = reflect(-sunVector, fsViewNormal);
				specularFactor = max(0.0, R.z);
			}

			specularFactor *= specularValue;
			specularFactor *= lighting[1].x;

			if (colData.a < 1.0) {
				/// Specular hi-light affects translucent polygons alpha channel ///
				finalData.a = max(finalData.a, specularFactor);
			}

			finalData.rgb += specularFactor;
		}
	}

	// Final clamp: we need it for proper shading in dimmed light and dark ambients
	finalData.rgb = min(finalData.rgb, vec3(1.0));

	// Spotlight on fog
	vec3 lSpotFogColor = spotFogColor * fogAttenuation * fogColour.rgb * lobeFogEffect;

	// Fog & spotlight applied
	finalData.rgb = mix(finalData.rgb, fogData.rgb + lSpotFogColor, fogData.a);

	// Write outputs to colour buffers
	WriteOutputs(finalData,colourLayer);
}

#include "R3DShaderCommon.glsl"
