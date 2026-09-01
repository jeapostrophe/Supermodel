// The Vulkan port of Src/Graphics/Shaders2D.h's s_fragmentShader.
//
// The filter bodies are verbatim. Four mechanical Vulkan edits:
//   * #version 450, explicit set/binding on the sampler and locations on the
//     varying and the output;
//   * CRender2D's runtime "#define UPSCALEMODE n" becomes a specialization
//     constant, so the #if selection becomes an if selection -- the compiler
//     folds it away when the pipeline is created, so the three variants cost
//     exactly what the three #define variants cost;
//   * the `sampler` parameter names are `tex`, because `sampler` is a type
//     keyword in Vulkan GLSL and cannot be an identifier.
#version 450

layout(constant_id = 0) const int UPSCALEMODE = 0;

// inputs
layout(set = 0, binding = 0) uniform sampler2D tex1;	// texture
layout(location = 0) in vec2 fsTexCoord;

// outputs
layout(location = 0) out vec4 fragColor;

// smoothstep instead of linear, remapping to a bilinear lookup
vec4 biquintic(sampler2D tex, vec2 uv)
{
	vec2 ts = vec2(textureSize(tex,0));
	uv = uv*ts + 0.5;

	vec2 i = floor(uv);
	vec2 f = uv - i;
	f = f*f*f*(f*(f*6.0-15.0)+10.0) - 0.5;

	uv = (i + f)/ts;
	return texture(tex, uv);
}

// 4x4 bicubic filter using 4 bilinear texture lookups
vec4 cubic(float v) {
	vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
	vec4 s = n * n * n;
	float x = s.x;
	float y = s.y - 4.0 * s.x;
	float z = s.z - 4.0 * s.y + 6.0 * s.x;
	float w = 6.0 - x - y - z;
	return vec4(x, y, z, w) * (1.0 / 6.0);
}
vec4 bicubic(sampler2D tex, vec2 uv) {
	vec2 invTexSize = 1.0 / vec2(textureSize(tex,0));
	uv = uv * vec2(textureSize(tex,0)) - 0.5;
	vec2 uvi = floor(uv);
	vec2 fxy = uv-uvi;
	vec4 xcubic = cubic(fxy.x);
	vec4 ycubic = cubic(fxy.y);
	vec4 c = uvi.xxyy + vec2(-0.5, 1.5).xyxy;
	vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
	vec4 offset = c + vec4(xcubic.yw, ycubic.yw) / s;
	offset *= invTexSize.xxyy;
	vec4 sample0 = texture(tex, offset.xz);
	vec4 sample1 = texture(tex, offset.yz);
	vec4 sample2 = texture(tex, offset.xw);
	vec4 sample3 = texture(tex, offset.yw);
	float sx = s.x / (s.x + s.y);
	float sy = s.z / (s.z + s.w);
	return mix(mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);
}

void main()
{
	if (UPSCALEMODE == 3)
		fragColor = bicubic(tex1, fsTexCoord);
	else if (UPSCALEMODE == 1)
		fragColor = vec4(biquintic(tex1, fsTexCoord).rgb, texture(tex1, fsTexCoord).a);
	else
		fragColor = texture(tex1, fsTexCoord);
}
