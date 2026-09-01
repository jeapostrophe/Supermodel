#ifndef _R3DSHADER_H_
#define _R3DSHADER_H_

#include "Util/NewConfig.h"
#include "Graphics/New3D/Model.h"
#include "Graphics/Vulkan/VulkanContext.h"
#include "Graphics/Vulkan/VulkanShader.h"
#include <cstdint>
#include <string>

namespace Vul3D {

using New3D::Layer;
using New3D::Mesh;
using New3D::Model;
using New3D::Viewport;

class R3DShader
{
public:
	R3DShader(const Util::Config::Node &config);

	bool	LoadShader			(const char* vertexShader = nullptr, const char* fragmentShader = nullptr);
	void	UnloadShader		();
	void	SetMeshUniforms		(const Mesh* m);
	void	SetModelStates		(const Model* model);
	void	SetViewportUniforms	(const Viewport *vp);
	void	Start				();
	void	SetShader			(bool enable = true);
	void	DiscardAlpha		(bool discard);				// use to remove alpha from texture alpha only polys for 1st pass
	void	SetLayer			(Layer layer);

	// -------------------------------------------------- Vulkan-only additions --
	//
	// A Vulkan pipeline bakes in the render pass, the colour write masks
	// (glDrawBuffers), the depth compare (glDepthFunc) and the stencil ops
	// (glStencilOp) that GL set as loose state, so the four GL calls that used
	// to do those become these. The state space is {layer} x {stencil mode} x
	// {depth compare} = twelve pipelines, all built up front.

	bool	CreatePipelines		(Vulkan::RenderTarget* target);
	void	SetDepthCompare		(VkCompareOp compare);			// glDepthFunc
	void	SetTextureBanks		(VkImageView bank0, VkImageView bank1);
	void	Draw				(int first, int count);			// glDrawArrays, preceded by the uniform flush

	// std140 mirrors of Shaders/R3DBlocks.glsl. Public only so that the .cpp's
	// static_asserts can take offsetof of their members; nothing outside
	// R3DShader builds one.
	struct ViewportBlock
	{
		float	projMat[16];
		float	lighting[2][4];		// vec3[2]: std140 pads each element to 16 bytes
		float	spotEllipse[4];
		float	fogColour[4];		// vec3 + std140 pad
		float	spotColor[4];		// ...
		float	spotFogColor[4];
		float	spotRange[2];
		float	fogDensity;
		float	fogStart;
		float	fogAttenuation;
		float	fogAmbient;
		float	cota;
		int32_t	hardwareStep;
		int32_t	sunClamp;
		int32_t	intensityClamp;
		int32_t	pad[2];
	};

	struct ModelBlock
	{
		float	modelMat[16];
		float	modelScale;
		float	nodeAlpha;
		float	pad[2];
	};

	struct MeshBlock
	{
		int32_t	baseTexInfo[4];
		int32_t	textureWrapMode[2];
		int32_t	texturePage;
		int32_t	baseTexType;
		int32_t	microTextureID;
		int32_t	colourLayer;
		float	microTextureMinLOD;
		float	fogIntensity;
		float	shininess;
		float	specularValue;
		int32_t	textureEnabled;
		int32_t	microTexture;
		int32_t	textureInverted;
		int32_t	textureAlpha;
		int32_t	alphaTest;
		int32_t	discardAlpha;
		int32_t	lightEnabled;
		int32_t	specularEnabled;
		int32_t	fixedShading;
		int32_t	smoothShading;
		int32_t	translatorMap;
		int32_t	polyAlpha;
		int32_t	pad[2];
	};

private:

	bool	EnsureDescriptorSet	();
	bool	Flush				();				// append the dirty blocks and rebind the set

	// run-time config
	const Util::Config::Node &m_config;

	// shader objects
	Vulkan::ShaderProgram m_program;

	// pipelines, indexed [layer][stencil mode][depth compare]
	VkPipeline m_pipelines[3][2][2] = {};

	// the CPU side of the three uniform blocks, and whether each needs
	// re-appending to this frame's uniform ring before the next draw
	ViewportBlock	m_viewportBlock = {};
	ModelBlock		m_modelBlock = {};
	MeshBlock		m_meshBlock = {};
	bool			m_dirtyViewportBlock;
	bool			m_dirtyModelBlock;
	bool			m_dirtyMeshBlock;

	// the descriptor set the three blocks and the two texture banks are bound
	// through, plus what it was written against (the frame's uniform ring and
	// the bank views), so a change of either re-allocates it
	VkDescriptorSet	m_set		= VK_NULL_HANDLE;
	VkBuffer		m_setBuffer	= VK_NULL_HANDLE;
	VkImageView		m_setBanks[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
	uint32_t		m_offsets[3] = { 0, 0, 0 };
	VkImageView		m_banks[2]	= { VK_NULL_HANDLE, VK_NULL_HANDLE };

	// cached mesh values
	bool	m_textured1;
	bool	m_textured2;
	bool	m_textureAlpha;		// use alpha in texture
	bool	m_alphaTest;		// discard fragment based on alpha (ogl does this with fixed function)
	float	m_fogIntensity;
	bool	m_lightEnabled;
	float	m_shininess;
	float	m_specularValue;
	bool	m_specularEnabled;
	bool	m_fixedShading;
	bool	m_smoothShading;
	bool	m_translatorMap;
	bool	m_polyAlpha;
	int		m_texturePage;

	bool	m_layered;
	bool	m_noLosReturn;
	float	m_microTexMinLOD;
	int		m_microTexID;
	int		m_baseTexInfo[4];
	int		m_baseTexType;
	int		m_texWrapMode[2];
	bool	m_textureInverted;

	// cached model values
	float	m_modelScale;
	float	m_nodeAlpha;
	int		m_transX;
	int		m_transY;
	int		m_transPage;

	// are our cache values dirty
	bool	m_dirtyMesh;
	bool	m_dirtyModel;

	// the pipeline selectors that were loose GL state
	Layer			m_layer;
	Vulkan::StencilMode	m_stencilMode;
	VkCompareOp		m_depthCompare;
};


} // Vul3D

#endif
