#include "R3DShader.h"

#include <cstddef>
#include <cstring>

#include "Supermodel.h"
#include "Shaders/R3DShaderTriangles.vert.spv.h"
#include "Shaders/R3DShaderTriangles.frag.spv.h"
#include "Shaders/R3DBlockOffsets.h"

// The quad path is not ported: Metal has no geometry stage. CVul3D's
// constructor logs once when the config asks for it and renders triangles.

namespace Vul3D {

using Vulkan::VulkanContext;
using Vulkan::VulkanShader;

// The GLSL is the source of truth: R3DBlockOffsets.h is generated out of the
// compiled SPIR-V by Scripts/build-spirv.sh, so these compare the C++ mirrors
// against the shader rather than against a hand-typed copy of it.
static_assert(offsetof(R3DShader::ViewportBlock, projMat)			== VUL3D_STD140_ViewportBlock_projMat,			"ViewportBlock.projMat");
static_assert(offsetof(R3DShader::ViewportBlock, lighting)			== VUL3D_STD140_ViewportBlock_lighting,			"ViewportBlock.lighting");
static_assert(offsetof(R3DShader::ViewportBlock, spotEllipse)		== VUL3D_STD140_ViewportBlock_spotEllipse,		"ViewportBlock.spotEllipse");
static_assert(offsetof(R3DShader::ViewportBlock, fogColour)			== VUL3D_STD140_ViewportBlock_fogColour,		"ViewportBlock.fogColour");
static_assert(offsetof(R3DShader::ViewportBlock, spotColor)			== VUL3D_STD140_ViewportBlock_spotColor,		"ViewportBlock.spotColor");
static_assert(offsetof(R3DShader::ViewportBlock, spotFogColor)		== VUL3D_STD140_ViewportBlock_spotFogColor,		"ViewportBlock.spotFogColor");
static_assert(offsetof(R3DShader::ViewportBlock, spotRange)			== VUL3D_STD140_ViewportBlock_spotRange,		"ViewportBlock.spotRange");
static_assert(offsetof(R3DShader::ViewportBlock, fogDensity)		== VUL3D_STD140_ViewportBlock_fogDensity,		"ViewportBlock.fogDensity");
static_assert(offsetof(R3DShader::ViewportBlock, fogStart)			== VUL3D_STD140_ViewportBlock_fogStart,			"ViewportBlock.fogStart");
static_assert(offsetof(R3DShader::ViewportBlock, fogAttenuation)	== VUL3D_STD140_ViewportBlock_fogAttenuation,	"ViewportBlock.fogAttenuation");
static_assert(offsetof(R3DShader::ViewportBlock, fogAmbient)		== VUL3D_STD140_ViewportBlock_fogAmbient,		"ViewportBlock.fogAmbient");
static_assert(offsetof(R3DShader::ViewportBlock, cota)				== VUL3D_STD140_ViewportBlock_cota,				"ViewportBlock.cota");
static_assert(offsetof(R3DShader::ViewportBlock, hardwareStep)		== VUL3D_STD140_ViewportBlock_hardwareStep,		"ViewportBlock.hardwareStep");
static_assert(offsetof(R3DShader::ViewportBlock, sunClamp)			== VUL3D_STD140_ViewportBlock_sunClamp,			"ViewportBlock.sunClamp");
static_assert(offsetof(R3DShader::ViewportBlock, intensityClamp)	== VUL3D_STD140_ViewportBlock_intensityClamp,	"ViewportBlock.intensityClamp");
// ...and that intensityClamp really is the LAST member, which is what catches a
// member appended to the GLSL: an append moves no existing offset, so every
// assert above would still pass while this mirror was silently short.
static_assert(offsetof(R3DShader::ViewportBlock, intensityClamp)	== VUL3D_STD140_ViewportBlock_LASTOFFSET,		"ViewportBlock has a member this mirror does not");
static_assert(sizeof(R3DShader::ViewportBlock) % 16 == 0, "std140 blocks are a multiple of 16 bytes");

static_assert(offsetof(R3DShader::ModelBlock, modelMat)				== VUL3D_STD140_ModelBlock_modelMat,			"ModelBlock.modelMat");
static_assert(offsetof(R3DShader::ModelBlock, modelScale)			== VUL3D_STD140_ModelBlock_modelScale,			"ModelBlock.modelScale");
static_assert(offsetof(R3DShader::ModelBlock, nodeAlpha)			== VUL3D_STD140_ModelBlock_nodeAlpha,			"ModelBlock.nodeAlpha");
static_assert(offsetof(R3DShader::ModelBlock, nodeAlpha)			== VUL3D_STD140_ModelBlock_LASTOFFSET,			"ModelBlock has a member this mirror does not");
static_assert(sizeof(R3DShader::ModelBlock) % 16 == 0, "std140 blocks are a multiple of 16 bytes");

static_assert(offsetof(R3DShader::MeshBlock, baseTexInfo)			== VUL3D_STD140_MeshBlock_baseTexInfo,			"MeshBlock.baseTexInfo");
static_assert(offsetof(R3DShader::MeshBlock, textureWrapMode)		== VUL3D_STD140_MeshBlock_textureWrapMode,		"MeshBlock.textureWrapMode");
static_assert(offsetof(R3DShader::MeshBlock, texturePage)			== VUL3D_STD140_MeshBlock_texturePage,			"MeshBlock.texturePage");
static_assert(offsetof(R3DShader::MeshBlock, baseTexType)			== VUL3D_STD140_MeshBlock_baseTexType,			"MeshBlock.baseTexType");
static_assert(offsetof(R3DShader::MeshBlock, microTextureID)		== VUL3D_STD140_MeshBlock_microTextureID,		"MeshBlock.microTextureID");
static_assert(offsetof(R3DShader::MeshBlock, colourLayer)			== VUL3D_STD140_MeshBlock_colourLayer,			"MeshBlock.colourLayer");
static_assert(offsetof(R3DShader::MeshBlock, microTextureMinLOD)	== VUL3D_STD140_MeshBlock_microTextureMinLOD,	"MeshBlock.microTextureMinLOD");
static_assert(offsetof(R3DShader::MeshBlock, fogIntensity)			== VUL3D_STD140_MeshBlock_fogIntensity,			"MeshBlock.fogIntensity");
static_assert(offsetof(R3DShader::MeshBlock, shininess)				== VUL3D_STD140_MeshBlock_shininess,			"MeshBlock.shininess");
static_assert(offsetof(R3DShader::MeshBlock, specularValue)			== VUL3D_STD140_MeshBlock_specularValue,		"MeshBlock.specularValue");
static_assert(offsetof(R3DShader::MeshBlock, textureEnabled)		== VUL3D_STD140_MeshBlock_textureEnabled,		"MeshBlock.textureEnabled");
static_assert(offsetof(R3DShader::MeshBlock, microTexture)			== VUL3D_STD140_MeshBlock_microTexture,			"MeshBlock.microTexture");
static_assert(offsetof(R3DShader::MeshBlock, textureInverted)		== VUL3D_STD140_MeshBlock_textureInverted,		"MeshBlock.textureInverted");
static_assert(offsetof(R3DShader::MeshBlock, textureAlpha)			== VUL3D_STD140_MeshBlock_textureAlpha,			"MeshBlock.textureAlpha");
static_assert(offsetof(R3DShader::MeshBlock, alphaTest)				== VUL3D_STD140_MeshBlock_alphaTest,			"MeshBlock.alphaTest");
static_assert(offsetof(R3DShader::MeshBlock, discardAlpha)			== VUL3D_STD140_MeshBlock_discardAlpha,			"MeshBlock.discardAlpha");
static_assert(offsetof(R3DShader::MeshBlock, lightEnabled)			== VUL3D_STD140_MeshBlock_lightEnabled,			"MeshBlock.lightEnabled");
static_assert(offsetof(R3DShader::MeshBlock, specularEnabled)		== VUL3D_STD140_MeshBlock_specularEnabled,		"MeshBlock.specularEnabled");
static_assert(offsetof(R3DShader::MeshBlock, fixedShading)			== VUL3D_STD140_MeshBlock_fixedShading,			"MeshBlock.fixedShading");
static_assert(offsetof(R3DShader::MeshBlock, smoothShading)			== VUL3D_STD140_MeshBlock_smoothShading,		"MeshBlock.smoothShading");
static_assert(offsetof(R3DShader::MeshBlock, translatorMap)			== VUL3D_STD140_MeshBlock_translatorMap,		"MeshBlock.translatorMap");
static_assert(offsetof(R3DShader::MeshBlock, polyAlpha)				== VUL3D_STD140_MeshBlock_polyAlpha,			"MeshBlock.polyAlpha");
static_assert(offsetof(R3DShader::MeshBlock, polyAlpha)				== VUL3D_STD140_MeshBlock_LASTOFFSET,			"MeshBlock has a member this mirror does not");
static_assert(sizeof(R3DShader::MeshBlock) % 16 == 0, "std140 blocks are a multiple of 16 bytes");

R3DShader::R3DShader(const Util::Config::Node &config)
{
	// New3D kept `config` because LoadShader chose between the triangle and the
	// quad shader with it. There is only one shader here, so nothing reads it;
	// the parameter stays so that CVul3D's construction of this member is the
	// line New3D has.
	(void)config;

	Start();	// reset attributes
}

void R3DShader::Start()
{
	m_textured1			= false;
	m_textured2			= false;
	m_textureAlpha		= false;		// use alpha in texture
	m_alphaTest			= false;		// discard fragment based on alpha (ogl does this with fixed function)
	m_lightEnabled		= false;
	m_specularEnabled	= false;
	m_layered			= false;
	m_noLosReturn		= false;
	m_textureInverted	= false;
	m_fixedShading		= false;
	m_smoothShading		= false;
	m_translatorMap		= false;
	m_modelScale		= 1.0f;
	m_nodeAlpha			= 1.0f;
	m_shininess			= 0;
	m_specularValue		= 0;
	m_microTexMinLOD	= 0;
	m_fogIntensity		= 0.0f;
	m_microTexID		= -1;
	m_texturePage		= -1;

	m_baseTexInfo[0]	= -1;
	m_baseTexInfo[1]	= -1;
	m_baseTexInfo[2]	= -1;
	m_baseTexInfo[3]	= -1;

	m_baseTexType		= -1;

	m_transX			= -1;
	m_transY			= -1;
	m_transPage			= -1;

	m_texWrapMode[0]	= 0;
	m_texWrapMode[1]	= 0;

	m_dirtyMesh			= true;			// dirty means all the above are dirty, ie first run
	m_dirtyModel		= true;

	// The GL program held its uniform values; here they live in three blocks
	// that have to be pushed into the frame's uniform ring, so resetting the
	// cache also means every block has to go out again.
	m_dirtyViewportBlock	= true;
	m_dirtyModelBlock		= true;
	m_dirtyMeshBlock		= true;

	m_layer			= Layer::colour;
	m_stencilMode	= Vulkan::StencilMode::Los;
	m_depthCompare	= VK_COMPARE_OP_GREATER_OR_EQUAL;
}

bool R3DShader::LoadShader()
{
	VulkanContext &context = VulkanContext::Get();

	m_program.vertexShader		= VulkanShader::CreateModule(context, R3DShaderTriangles_vert_spv, sizeof(R3DShaderTriangles_vert_spv));
	m_program.fragmentShader	= VulkanShader::CreateModule(context, R3DShaderTriangles_frag_spv, sizeof(R3DShaderTriangles_frag_spv));

	// What glGetUniformLocation used to find by name is fixed by the shader's
	// own layout qualifiers now, so this is the whole of it: three std140
	// blocks bound by dynamic offset, and the two texture sheets.
	const Vulkan::DescriptorBinding bindings[] = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1 },	// ViewportBlock
		{ 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1 },	// ModelBlock
		{ 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1 },	// MeshBlock
		{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2 }								// textureBank[2]
	};
	m_program.setLayout			= context.CreateDescriptorSetLayout(bindings, 4);
	m_program.pipelineLayout	= VulkanShader::CreateLayout(context, &m_program.setLayout, 1, 0, 0);

	return m_program.vertexShader != VK_NULL_HANDLE && m_program.fragmentShader != VK_NULL_HANDLE &&
		m_program.setLayout != VK_NULL_HANDLE && m_program.pipelineLayout != VK_NULL_HANDLE;
}

bool R3DShader::CreatePipelines(Vulkan::RenderTarget* target)
{
	VulkanContext &context = VulkanContext::Get();

	if (target == nullptr || m_program.vertexShader == VK_NULL_HANDLE) {
		return false;
	}

	for (auto &byLayer : m_pipelines) {
		for (auto &byStencil : byLayer) {
			for (auto &pipeline : byStencil) {
				VulkanShader::DestroyPipeline(context, pipeline);
				pipeline = VK_NULL_HANDLE;
			}
		}
	}

	Vulkan::PipelineDesc desc;
	desc.vertexShader	= m_program.vertexShader;
	desc.fragmentShader	= m_program.fragmentShader;
	desc.layout			= m_program.pipelineLayout;
	desc.renderPass		= target->pass;
	desc.colourAttachmentCount = (uint32_t)target->colour.size();
	desc.topology		= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	desc.blend			= false;		// glDisable(GL_BLEND)
	desc.depthTest		= true;			// glEnable(GL_DEPTH_TEST)
	desc.depthWrite		= true;			// glDepthMask(GL_TRUE)

	// glVertexAttribPointer, once, in the pipeline. The locations are the ones
	// Shaders/R3DShaderTriangles.vert declares.
	desc.vertexStride	= (uint32_t)sizeof(New3D::FVertex);
	desc.attributes = {
		{ 0, VK_FORMAT_R32G32B32A32_SFLOAT,	(uint32_t)offsetof(New3D::FVertex, base.pos)		},	// inVertex
		{ 1, VK_FORMAT_R32G32B32_SFLOAT,	(uint32_t)offsetof(New3D::FVertex, base.normal)		},	// inNormal
		{ 2, VK_FORMAT_R32G32_SFLOAT,		(uint32_t)offsetof(New3D::FVertex, base.texcoords)	},	// inTexCoord
		{ 3, VK_FORMAT_R8G8B8A8_UNORM,		(uint32_t)offsetof(New3D::FVertex, faceColour)		},	// inColour (GL_UNSIGNED_BYTE, normalized)
		{ 4, VK_FORMAT_R32G32B32_SFLOAT,	(uint32_t)offsetof(New3D::FVertex, faceNormal)		},	// inFaceNormal
		{ 5, VK_FORMAT_R32_SFLOAT,			(uint32_t)offsetof(New3D::FVertex, base.fixedShade)	},	// inFixedShade
		{ 6, VK_FORMAT_R32_SFLOAT,			(uint32_t)offsetof(New3D::FVertex, textureNP)		}	// inTextureNP
	};

	static const VkColorComponentFlags rgba =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	bool okay = true;

	for (size_t layer = 0; layer < PIPELINE_LAYERS; layer++) {

		// R3DFrameBuffers::SetFBO's glDrawBuffers: the colour layer writes all
		// three attachments, each transparency layer only its own.
		for (size_t i = 0; i < std::size(desc.writeMask); i++) {
			desc.writeMask[i] = (layer == (size_t)Layer::colour || layer == i) ? rgba : 0;
		}

		for (size_t stencil = 0; stencil < std::size(PIPELINE_STENCIL_MODES); stencil++) {

			desc.stencil = PIPELINE_STENCIL_MODES[stencil];

			for (size_t compare = 0; compare < std::size(PIPELINE_DEPTH_COMPARES); compare++) {

				desc.depthCompare = PIPELINE_DEPTH_COMPARES[compare];

				m_pipelines[layer][stencil][compare] = VulkanShader::CreatePipeline(context, desc);
				okay = okay && (m_pipelines[layer][stencil][compare] != VK_NULL_HANDLE);
			}
		}
	}

	if (!okay) {
		ErrorLog("R3DShader: could not create its pipelines.\n");
	}

	return okay;
}

void R3DShader::UnloadShader()
{
	VulkanContext &context = VulkanContext::Get();

	for (auto &byLayer : m_pipelines) {
		for (auto &byStencil : byLayer) {
			for (auto &pipeline : byStencil) {
				VulkanShader::DestroyPipeline(context, pipeline);
				pipeline = VK_NULL_HANDLE;
			}
		}
	}

	m_program.Destroy(context);		// m_program.pipeline is always null: the twelve above are this shader's pipelines
	m_set = VK_NULL_HANDLE;
	m_setBuffer = VK_NULL_HANDLE;
}

void R3DShader::SetShader(bool enable)
{
	if (enable) {
		// glUseProgram's Vulkan half is binding a pipeline, which happens per
		// draw; what is left of it here is the state reset.
		/*
		 * Drop the set. This is LOAD-BEARING, not tidiness: VulkanContext::Begin
		 * resets the frame slot's descriptor pool, and with a single frame slot
		 * the uniform ring's VkBuffer handle is IDENTICAL from frame to frame --
		 * so EnsureDescriptorSet's `m_setBuffer == ring` test would happily
		 * reuse a set the pool has already freed. SetRenderStates calls this
		 * once per pass rather than once per frame, which is more often than
		 * strictly needed and cheaper than tracking the frame.
		 */
		m_set = VK_NULL_HANDLE;
		Start();
		DiscardAlpha(false);	// need some default
	}
}

void R3DShader::SetTextureBanks(VkImageView bank0, VkImageView bank1)
{
	m_banks[0] = bank0;
	m_banks[1] = bank1;
}

void R3DShader::SetDepthCompare(VkCompareOp compare)
{
	m_depthCompare = compare;
}

void R3DShader::SetMeshUniforms(const Mesh* m)
{
	VulkanContext &context = VulkanContext::Get();

	if (m == nullptr) {
		return;			// sanity check
	}

	// textureBank[0] and [1] were sampler uniforms naming texture units; they
	// are descriptor bindings now and EnsureDescriptorSet writes them.

	if (m_dirtyMesh || m->textured != m_textured1) {
		m_meshBlock.textureEnabled = m->textured;
		m_dirtyMeshBlock = true;
		m_textured1 = m->textured;
	}

	if (m_dirtyMesh || m->microTexture != m_textured2) {
		m_meshBlock.microTexture = m->microTexture;
		m_dirtyMeshBlock = true;
		m_textured2 = m->microTexture;
	}

	if (m_dirtyMesh || (m->page ^ m_transPage) != m_texturePage) {
		m_meshBlock.texturePage = m->page ^ m_transPage;
		m_dirtyMeshBlock = true;
		m_texturePage = (m->page ^ m_transPage);
	}

	if (m_dirtyMesh || m->microTextureMinLOD != m_microTexMinLOD) {
		m_meshBlock.microTextureMinLOD = m->microTextureMinLOD;
		m_dirtyMeshBlock = true;
		m_microTexMinLOD = m->microTextureMinLOD;
	}

	if (m_dirtyMesh || m->microTextureID != m_microTexID) {
		m_meshBlock.microTextureID = m->microTextureID;
		m_dirtyMeshBlock = true;
		m_microTexID = m->microTextureID;
	}

	if (m_dirtyMesh || (m_baseTexInfo[0] != m->x || m_baseTexInfo[1] != m->y) || m_baseTexInfo[2] != m->width || m_baseTexInfo[3] != m->height) {

		m_baseTexInfo[0] = m->x;
		m_baseTexInfo[1] = m->y;
		m_baseTexInfo[2] = m->width;
		m_baseTexInfo[3] = m->height;

		m_meshBlock.baseTexInfo[0] = (m->x + m_transX);
		m_meshBlock.baseTexInfo[1] = (m->y + m_transY);
		m_meshBlock.baseTexInfo[2] = m->width;
		m_meshBlock.baseTexInfo[3] = m->height;
		m_dirtyMeshBlock = true;
	}

	if (m_dirtyMesh || m_baseTexType != m->format) {
		m_baseTexType = m->format;
		m_meshBlock.baseTexType = m_baseTexType;
		m_dirtyMeshBlock = true;
	}

	if (m_dirtyMesh || m->inverted != m_textureInverted) {
		m_meshBlock.textureInverted = m->inverted;
		m_dirtyMeshBlock = true;
		m_textureInverted = m->inverted;
	}

	if (m_dirtyMesh || m->alphaTest != m_alphaTest) {
		m_meshBlock.alphaTest = m->alphaTest;
		m_dirtyMeshBlock = true;
		m_alphaTest = m->alphaTest;
	}

	if (m_dirtyMesh || m->textureAlpha != m_textureAlpha) {
		m_meshBlock.textureAlpha = m->textureAlpha;
		m_dirtyMeshBlock = true;
		m_textureAlpha = m->textureAlpha;
	}

	if (m_dirtyMesh || m->fogIntensity != m_fogIntensity) {
		m_meshBlock.fogIntensity = m->fogIntensity;
		m_dirtyMeshBlock = true;
		m_fogIntensity = m->fogIntensity;
	}

	if (m_dirtyMesh || m->lighting != m_lightEnabled) {
		m_meshBlock.lightEnabled = m->lighting;
		m_dirtyMeshBlock = true;
		m_lightEnabled = m->lighting;
	}

	if (m_dirtyMesh || m->shininess != m_shininess) {
		m_meshBlock.shininess = m->shininess;
		m_dirtyMeshBlock = true;
		m_shininess = m->shininess;
	}

	if (m_dirtyMesh || m->specular != m_specularEnabled) {
		m_meshBlock.specularEnabled = m->specular;
		m_dirtyMeshBlock = true;
		m_specularEnabled = m->specular;
	}

	if (m_dirtyMesh || m->specularValue != m_specularValue) {
		m_meshBlock.specularValue = m->specularValue;
		m_dirtyMeshBlock = true;
		m_specularValue = m->specularValue;
	}

	if (m_dirtyMesh || m->fixedShading != m_fixedShading) {
		m_meshBlock.fixedShading = m->fixedShading;
		m_dirtyMeshBlock = true;
		m_fixedShading = m->fixedShading;
	}

	if (m_dirtyMesh || m->smoothShading != m_smoothShading) {
		m_meshBlock.smoothShading = m->smoothShading;
		m_dirtyMeshBlock = true;
		m_smoothShading = m->smoothShading;
	}

	if (m_dirtyMesh || m->translatorMap != m_translatorMap) {
		m_meshBlock.translatorMap = m->translatorMap;
		m_dirtyMeshBlock = true;
		m_translatorMap = m->translatorMap;
	}

	if (m_dirtyMesh || m->polyAlpha != m_polyAlpha) {
		m_meshBlock.polyAlpha = m->polyAlpha;
		m_dirtyMeshBlock = true;
		m_polyAlpha = m->polyAlpha;
	}

	if (m_dirtyMesh || m->wrapModeU != m_texWrapMode[0] || m->wrapModeV != m_texWrapMode[1]) {
		m_texWrapMode[0] = m->wrapModeU;
		m_texWrapMode[1] = m->wrapModeV;
		m_meshBlock.textureWrapMode[0] = m_texWrapMode[0];
		m_meshBlock.textureWrapMode[1] = m_texWrapMode[1];
		m_dirtyMeshBlock = true;
	}

	// The GL trio glStencilFunc / glStencilOp / glStencilMask splits in two
	// under Vulkan: the reference and the two masks are dynamic state, and the
	// three ops are baked into the pipeline, which is why m_stencilMode is
	// assigned alongside every SetStencil. The branch structure is New3D's
	// unchanged -- including that the first branch writes the line-of-sight
	// state whether or not the mesh is layered.

	if (m_dirtyMesh || m->noLosReturn != m_noLosReturn) {
		m_noLosReturn = m->noLosReturn;
		m_stencilMode = Vulkan::StencilMode::Los;						// glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE)
		context.SetStencil((uint32_t)m_noLosReturn << 7, 0b10000000, 0b10000000);
	}

	if (m_dirtyMesh || m->layered != m_layered) {
		m_layered = m->layered;
		// i think it should just disable z write, but the polys I think must be written first
		if (m_layered) {
			m_stencilMode = Vulkan::StencilMode::Layered;				// basically stencil test passes if the value is zero,
			context.SetStencil(0, 0b01111111, 0b01111111);				// and if it passes we increment the value
		}
		else {
			m_stencilMode = Vulkan::StencilMode::Los;
			context.SetStencil((uint32_t)m_noLosReturn << 7, 0b10000000, 0b10000000);
		}
	}

	m_dirtyMesh = false;
}

void R3DShader::SetViewportUniforms(const Viewport *vp)
{
	//didn't bother caching these, they don't get frequently called anyway
	m_viewportBlock.fogDensity		= vp->fogDensity;
	m_viewportBlock.fogStart		= vp->fogStart;
	std::memcpy(m_viewportBlock.fogColour, vp->fogColour, sizeof(vp->fogColour));
	m_viewportBlock.fogAttenuation	= vp->fogAttenuation;
	m_viewportBlock.fogAmbient		= vp->fogAmbient;

	// glUniform3fv(loc, 2, ...) packed two vec3s tightly; std140 pads each
	// element of a vec3 array out to sixteen bytes, so they are copied one at
	// a time rather than in one memcpy.
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 3; j++) {
			m_viewportBlock.lighting[i][j] = vp->lightingParams[i * 3 + j];
		}
	}
	m_viewportBlock.sunClamp		= vp->sunClamp;
	m_viewportBlock.intensityClamp	= vp->intensityClamp;
	std::memcpy(m_viewportBlock.spotEllipse,	vp->spotEllipse,	sizeof(vp->spotEllipse));
	std::memcpy(m_viewportBlock.spotRange,		vp->spotRange,		sizeof(vp->spotRange));
	std::memcpy(m_viewportBlock.spotColor,		vp->spotColor,		sizeof(vp->spotColor));
	std::memcpy(m_viewportBlock.spotFogColor,	vp->spotFogColor,	sizeof(vp->spotFogColor));

	std::memcpy(m_viewportBlock.projMat, (const float *)vp->projectionMatrix, sizeof(m_viewportBlock.projMat));

	m_viewportBlock.hardwareStep	= vp->hardwareStep;

	m_viewportBlock.cota			= vp->cota;

	m_dirtyViewportBlock = true;
}

void R3DShader::SetModelStates(const Model* model)
{
	if (m_dirtyModel || model->scale != m_modelScale) {
		m_modelBlock.modelScale = model->scale;
		m_dirtyModelBlock = true;
		m_modelScale = model->scale;
	}

	if (m_dirtyModel || model->alpha != m_nodeAlpha) {
		m_modelBlock.nodeAlpha = model->alpha;
		m_dirtyModelBlock = true;
		m_nodeAlpha = model->alpha;
	}

	m_transX = model->textureOffsetX;
	m_transY = model->textureOffsetY;
	m_transPage = model->page;

	// reset texture values
	for (auto& i : m_baseTexInfo) { i = -1; }

	std::memcpy(m_modelBlock.modelMat, model->modelMat, sizeof(m_modelBlock.modelMat));
	m_dirtyModelBlock = true;

	m_dirtyModel = false;
}

void R3DShader::DiscardAlpha(bool discard)
{
	m_meshBlock.discardAlpha = discard;
	m_dirtyMeshBlock = true;
}

void R3DShader::SetLayer(Layer layer)
{
	m_meshBlock.colourLayer = (int32_t)layer;
	m_dirtyMeshBlock = true;

	// ...and the layer also picks the pipeline whose colour write masks are
	// R3DFrameBuffers::SetFBO's glDrawBuffers. Every SetLayer in CVul3D is
	// immediately followed by the matching SetFBO, so the two cannot diverge.
	m_layer = layer;
}

bool R3DShader::EnsureDescriptorSet()
{
	VulkanContext &context = VulkanContext::Get();

	VkBuffer ring = context.UniformRingBuffer();
	if (ring == VK_NULL_HANDLE || m_banks[0] == VK_NULL_HANDLE || m_banks[1] == VK_NULL_HANDLE) {
		return false;
	}

	if (m_set != VK_NULL_HANDLE && m_setBuffer == ring && m_setBanks[0] == m_banks[0] && m_setBanks[1] == m_banks[1]) {
		return true;
	}

	// A set is never updated after it has been bound -- it is replaced. The
	// pool it comes from is this frame's and is recycled behind the fence.
	m_set = context.AllocateDescriptorSet(m_program.setLayout);
	if (m_set == VK_NULL_HANDLE) {
		return false;
	}

	context.UpdateDescriptorBuffer(m_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, ring, sizeof(ViewportBlock));
	context.UpdateDescriptorBuffer(m_set, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, ring, sizeof(ModelBlock));
	context.UpdateDescriptorBuffer(m_set, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, ring, sizeof(MeshBlock));
	context.UpdateDescriptorImages(m_set, 3, m_banks, 2, context.Sampler(false));

	m_setBuffer		= ring;
	m_setBanks[0]	= m_banks[0];
	m_setBanks[1]	= m_banks[1];

	// The offsets recorded against the previous set point into a ring this set
	// does not name, so all three blocks have to be written out again.
	m_dirtyViewportBlock	= true;
	m_dirtyModelBlock		= true;
	m_dirtyMeshBlock		= true;

	return true;
}

bool R3DShader::Flush()
{
	VulkanContext &context = VulkanContext::Get();

	if (!EnsureDescriptorSet()) {
		return false;
	}

	struct { bool *dirty; const void *data; size_t size; } blocks[3] = {
		{ &m_dirtyViewportBlock,	&m_viewportBlock,	sizeof(m_viewportBlock)	},
		{ &m_dirtyModelBlock,		&m_modelBlock,		sizeof(m_modelBlock)	},
		{ &m_dirtyMeshBlock,		&m_meshBlock,		sizeof(m_meshBlock)		}
	};

	for (int i = 0; i < 3; i++) {

		if (!*blocks[i].dirty) {
			continue;
		}

		Vulkan::UniformSlice slice = context.AppendUniforms(blocks[i].data, blocks[i].size);

		// A slice that did not land in this frame's ring came from a dedicated
		// oversized buffer, which the set does not name -- skipping the draw is
		// the only honest thing left, and the ring is sized so it cannot happen.
		if (!slice.valid || slice.buffer != m_setBuffer) {
			return false;
		}

		m_offsets[i] = slice.offset;
		*blocks[i].dirty = false;
	}

	context.BindDescriptorSet(m_program.pipelineLayout, m_set, 3, m_offsets);
	return true;
}

/* The index of `value` in `table`, or -1: the inverse of CreatePipelines' loops. */
template <typename T, size_t N>
static int IndexIn(const T (&table)[N], T value)
{
	for (size_t i = 0; i < N; i++) {
		if (table[i] == value) {
			return (int)i;
		}
	}
	return -1;
}

void R3DShader::Draw(int first, int count)
{
	const size_t layer	= (size_t)m_layer;
	const int stencil	= IndexIn(PIPELINE_STENCIL_MODES, m_stencilMode);
	const int compare	= IndexIn(PIPELINE_DEPTH_COMPARES, m_depthCompare);

	// A state nothing built a pipeline for would otherwise draw with whichever
	// one index 0 happens to be, which is a wrong image rather than no image.
	if (layer >= PIPELINE_LAYERS || stencil < 0 || compare < 0) {
		static bool reported = false;
		if (!reported) {
			reported = true;
			ErrorLog("Vul3D: no pipeline for layer %d / stencil %d / depth compare %d; those polygons will not be drawn.\n",
				(int)m_layer, (int)m_stencilMode, (int)m_depthCompare);
		}
		return;
	}

	if (!Flush()) {
		return;
	}

	VulkanContext::Get().Draw(m_pipelines[layer][stencil][compare], (uint32_t)first, (uint32_t)count);
}


} // Vul3D
