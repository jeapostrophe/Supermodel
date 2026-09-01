// The Vulkan port of SuperAA::SuperAA's fragmentShader (Src/Graphics/SuperAA.cpp).
//
// Every number below -- the gammas and all five colour matrices -- is verbatim
// from the GL source, comments included. Two mechanical Vulkan edits:
//   * #version 450, explicit set/binding on the sampler, explicit locations;
//   * the two values the GL path pastes in as text ("const int aa = N;" and
//     "#define CRTCOLORS n") become specialization constants, so the #if chains
//     that selected cgamma and colmatrix become functions returning them. The
//     compiler folds the whole chain when the pipeline is created.
//
// `aa` is declared and unused, exactly as in the GL shader: the supersample
// resolve is the filtered texture fetch itself, not a loop in here.
#version 450

layout(constant_id = 0) const int aa = 1;
layout(constant_id = 1) const int CRTCOLORS = 0;

// inputs
layout(set = 0, binding = 0) uniform sampler2D tex1;	// base tex
layout(location = 0) in vec2 texCoord;

// outputs
layout(location = 0) out vec4 fragColor;

float cgammaValue()
{
	if (CRTCOLORS == 1)
		return 2.5; // what is the 'real' gamma/EOTF of a japanese arcade CRT of that time? 2.4 or 2.5 is what the net so far agrees on
	else if (CRTCOLORS == 2)
		return 2.25;
	else
		return -1.0;
}

// see https://www.retrorgb.com/colour-malarkey.html
// and accompanying matrices https://github.com/danmons/colour_matrix_adaptations/tree/main/csv
mat3 colmatrixValue()
{
	if (CRTCOLORS == 1)
		return mat3(1.3272128334714093, // ARI(D93/9300K) (JP) -> sRGB(D65/6500K) // what can be found in some of the sega or corresponding CRT manuals of the time is a range of ~9700K-10600K, so even cooler than D93
					-0.3802108879412366,
					-0.1003607696463202,
					-0.0241848600268417,
					0.9544506228550125,
					0.0807358585208939,
					-0.0239585810555497,
					-0.0409736005706461,
					1.4076563728858242);
	else if (CRTCOLORS == 2)
		return mat3(0.9241392201737613, // PVM_20M2U(D93/9300K) (JP) -> sRGB(D65/6500K), gamma 2.25
					-0.0690701363506526,
					-0.0084279079392561,
					0.0231962420140721,
					0.9729221778325590,
					0.0148832015024337,
					-0.0054875731998806,
					0.0098220960740544,
					1.3383896683854550);
	else if (CRTCOLORS == 3)
		return mat3(0.7822490684754086, // BT601_525_D93 (JP) -> sRGB(D65/6500K), gamma sRGB
					0.0506168576073398,
					0.0137752498011040,
					0.0147968947158740,
					0.9741745313046067,
					0.0220301953285839,
					-0.0013501205469208,
					-0.0044076726925543,
					1.3484819844991036);
	else if (CRTCOLORS == 4)
		return mat3(0.9395420637732393, // BT601_525 (US) -> sRGB(D65/6500K), gamma sRGB // extremely subtle
					0.0501813568598678,
					0.0102765793668928,
					0.0177722231435608,
					0.9657928624969044,
					0.0164349143595347,
					-0.0016215999431855,
					-0.0043697496597356,
					1.0059913496029214);
	else if (CRTCOLORS == 5)
		return mat3(1.0440432087628346, // BT601_625 (EUR,AUS) -> sRGB(D65/6500K), gamma sRGB // barely noticeable at all
					-0.0440432087628348,
					0.0000000000000001,
					0.0000000000000000,
					1.0000000000000002,
					0.0000000000000000,
					0.0000000000000000,
					0.0117933782840052,
					0.9882066217159947);
	else
		return mat3(1.0);
}

vec3 GetTextureValue(sampler2D s)
{
	vec2 clampedCoord = clamp(texCoord, vec2(0.0), vec2(1.0));
	return texture(s, clampedCoord).rgb;
}

float sRGB(float color)
{
	if (color <= 0.0031308)
		return 12.92 * color;
	else
		return 1.055 * pow(color, 1.0/2.4) - 0.055;
}

float invsRGB(float color)
{
	if (color <= 0.04045) // 0.03928 ?
		return color * (1.0/12.92);
	else
		return pow(color * (1.0/1.055) + (0.055/1.055), 2.4);
}

void main()
{
	vec3 finalColor = GetTextureValue(tex1);

	if (CRTCOLORS != 0)
	{
		float cgamma = cgammaValue();
		// transform input space to linear space
		if (cgamma != -1.0)
			finalColor = pow(finalColor, vec3(cgamma));
		else
			finalColor = vec3(invsRGB(finalColor.r),invsRGB(finalColor.g),invsRGB(finalColor.b));
		// do color transformation
		finalColor *= colmatrixValue();
		// transform from linear to sRGB output space
		finalColor = vec3(sRGB(finalColor.r),sRGB(finalColor.g),sRGB(finalColor.b));
	}

	fragColor = vec4(finalColor, 1.0);
}
