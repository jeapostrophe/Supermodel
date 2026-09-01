/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

/*
 * VulkanContext.h
 *
 * The foundation the Vulkan renderers are written against: a GL-shaped state
 * machine over Vulkan. See Src/Graphics/Vul3D/README.md.
 *
 * Two rules the whole port follows and this file implements:
 *
 *  1. NOTHING IS LINKED. Every VkXxx entry point is a function pointer fetched
 *     through the libretro frontend's get_instance_proc_addr / the device's
 *     vkGetDeviceProcAddr (see VulkanContext::Api). The build defines
 *     VK_NO_PROTOTYPES, so a direct call would not compile.
 *
 *  2. ROW 0 IS GL'S BOTTOM ROW. Every viewport, scissor and texture coordinate
 *     stays verbatim from the GL sources, because Vulkan's viewport transform
 *     maps NDC y = -1 to framebuffer row 0 -- which is exactly GL's mapping if
 *     row 0 is called "bottom". The two conventions meet in exactly one place:
 *     the flipped vkCmdBlitImage in End().
 */

#ifndef INCLUDED_VULKAN_VULKANCONTEXT_H
#define INCLUDED_VULKAN_VULKANCONTEXT_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vulkan/vulkan.h"
#include "libretro_vulkan.h"

namespace Vulkan {

/*
 * The Vulkan entry points this port uses, in one list so that the declarations
 * and the loader cannot drift apart: a name added here is declared, loaded and
 * checked for null by the same X-macro expansion.
 */
#define VULKAN_INSTANCE_FUNCTIONS(X) \
	X(vkGetDeviceProcAddr) \
	X(vkGetPhysicalDeviceMemoryProperties) \
	X(vkGetPhysicalDeviceProperties) \
	X(vkGetPhysicalDeviceFormatProperties)

#define VULKAN_DEVICE_FUNCTIONS(X) \
	X(vkDeviceWaitIdle) \
	X(vkGetDeviceQueue) \
	X(vkQueueSubmit) \
	X(vkCreateCommandPool) \
	X(vkDestroyCommandPool) \
	X(vkResetCommandPool) \
	X(vkAllocateCommandBuffers) \
	X(vkBeginCommandBuffer) \
	X(vkEndCommandBuffer) \
	X(vkCreateFence) \
	X(vkDestroyFence) \
	X(vkWaitForFences) \
	X(vkResetFences) \
	X(vkCreateImage) \
	X(vkDestroyImage) \
	X(vkGetImageMemoryRequirements) \
	X(vkBindImageMemory) \
	X(vkCreateImageView) \
	X(vkDestroyImageView) \
	X(vkCreateBuffer) \
	X(vkDestroyBuffer) \
	X(vkGetBufferMemoryRequirements) \
	X(vkBindBufferMemory) \
	X(vkAllocateMemory) \
	X(vkFreeMemory) \
	X(vkMapMemory) \
	X(vkUnmapMemory) \
	X(vkCreateSampler) \
	X(vkDestroySampler) \
	X(vkCreateRenderPass) \
	X(vkDestroyRenderPass) \
	X(vkCreateFramebuffer) \
	X(vkDestroyFramebuffer) \
	X(vkCreateShaderModule) \
	X(vkDestroyShaderModule) \
	X(vkCreatePipelineLayout) \
	X(vkDestroyPipelineLayout) \
	X(vkCreateGraphicsPipelines) \
	X(vkDestroyPipeline) \
	X(vkCreateDescriptorSetLayout) \
	X(vkDestroyDescriptorSetLayout) \
	X(vkCreateDescriptorPool) \
	X(vkDestroyDescriptorPool) \
	X(vkResetDescriptorPool) \
	X(vkAllocateDescriptorSets) \
	X(vkUpdateDescriptorSets) \
	X(vkCmdBeginRenderPass) \
	X(vkCmdEndRenderPass) \
	X(vkCmdBindPipeline) \
	X(vkCmdBindDescriptorSets) \
	X(vkCmdBindVertexBuffers) \
	X(vkCmdDraw) \
	X(vkCmdSetViewport) \
	X(vkCmdSetScissor) \
	X(vkCmdSetStencilReference) \
	X(vkCmdSetStencilCompareMask) \
	X(vkCmdSetStencilWriteMask) \
	X(vkCmdClearAttachments) \
	X(vkCmdPipelineBarrier) \
	X(vkCmdCopyBuffer) \
	X(vkCmdCopyBufferToImage) \
	X(vkCmdCopyImage) \
	X(vkCmdCopyImageToBuffer) \
	X(vkCmdBlitImage) \
	X(vkCmdPushConstants)

struct Api
{
#define VULKAN_DECLARE_FUNCTION(name) PFN_##name name = nullptr;
	VULKAN_INSTANCE_FUNCTIONS(VULKAN_DECLARE_FUNCTION)
	VULKAN_DEVICE_FUNCTIONS(VULKAN_DECLARE_FUNCTION)
#undef VULKAN_DECLARE_FUNCTION
};

/*
 * An image plus the memory and view that go with it, plus the layout it is
 * currently in. The layout field is the whole reason this is a struct and not
 * a bare VkImage: the renderers never write a barrier, so something has to
 * remember what state each image was left in.
 */
struct Image
{
	VkImage				image		= VK_NULL_HANDLE;
	VkDeviceMemory		memory		= VK_NULL_HANDLE;
	VkImageView			view		= VK_NULL_HANDLE;
	VkFormat			format		= VK_FORMAT_UNDEFINED;
	VkImageAspectFlags	aspect		= 0;
	VkImageLayout		layout		= VK_IMAGE_LAYOUT_UNDEFINED;
	uint32_t			width		= 0;
	uint32_t			height		= 0;
	uint32_t			levels		= 1;

	bool Valid() const { return image != VK_NULL_HANDLE; }
};

struct Buffer
{
	VkBuffer		buffer	= VK_NULL_HANDLE;
	VkDeviceMemory	memory	= VK_NULL_HANDLE;
	VkDeviceSize	size	= 0;
	void			*mapped	= nullptr;			// non-null only for HOST_VISIBLE buffers

	bool Valid() const { return buffer != VK_NULL_HANDLE; }
};

/*
 * N colour attachments plus an optional depth/stencil attachment, a render pass
 * that LOADs and STOREs every one of them (so binding a target never discards
 * what is in it, exactly as glBindFramebuffer does not), and a framebuffer.
 */
class RenderTarget
{
public:
	std::vector<Image>	colour;
	Image				depth;					// depth.Valid() == false if the target has none
	VkRenderPass		pass		= VK_NULL_HANDLE;
	VkFramebuffer		framebuffer	= VK_NULL_HANDLE;
	uint32_t			width		= 0;
	uint32_t			height		= 0;

	bool HasDepth() const { return depth.Valid(); }
};

/* One binding of a descriptor set layout, in the order the shader declares them. */
struct DescriptorBinding
{
	uint32_t			binding;
	VkDescriptorType	type;
	VkShaderStageFlags	stages;
	uint32_t			count;
};

/* What UniformRing::Append hands back: a buffer plus the dynamic offset to bind it at. */
struct UniformSlice
{
	VkBuffer	buffer	= VK_NULL_HANDLE;
	uint32_t	offset	= 0;
	bool		valid	= false;
};

class VulkanContext
{
public:
	/*
	 * The one context, reached the way the GL renderers reach the GL context:
	 * globally. Everything below asserts nothing -- calls made before Init or
	 * after Shutdown are no-ops, because the libretro lifecycle really does
	 * call renderer teardown after the context is gone.
	 */
	static VulkanContext &Get();

	/*
	 * Init(iface):
	 *
	 * Adopts the frontend's instance, physical device, device and queue, loads
	 * the function table and builds the per-frame slots (one per bit set in
	 * get_sync_index_mask). Does NOT create the screen target -- the output
	 * size is not known yet; see CreateScreen.
	 */
	bool	Init(const retro_hw_render_interface_vulkan *iface);

	/*
	 * Shutdown():
	 *
	 * Waits for the device to go idle and destroys everything this context
	 * made. NEVER destroys the VkDevice: the frontend owns it.
	 */
	void	Shutdown();

	bool	IsReady() const { return m_ready; }

	const Api			&Vk() const		{ return m_vk; }
	VkDevice			Device() const	{ return m_device; }
	VkCommandBuffer		Cmd() const		{ return m_frameOpen ? m_slots[m_slot].cmd : VK_NULL_HANDLE; }
	uint32_t			SlotIndex() const { return m_slot; }
	uint32_t			SlotCount() const { return (uint32_t)m_slots.size(); }
	bool				FrameOpen() const { return m_frameOpen; }

	// ---------------------------------------------------------------- frame --

	/*
	 * Begin(width, height):
	 *
	 * Top of retro_run. Picks up the frontend's sync index, waits on the fence
	 * of the frame that last used that slot, recycles the slot's command pool,
	 * descriptor pool and rings, and opens the primary command buffer. Rebuilds
	 * the screen target and the presentation images if the output size moved.
	 */
	bool	Begin(uint32_t width, uint32_t height);

	/*
	 * End():
	 *
	 * Bottom of retro_run. Ends any open render pass, blits the screen target
	 * into this slot's presentation image FLIPPED (the one place the GL and
	 * Vulkan row orders meet), leaves it in SHADER_READ_ONLY_OPTIMAL, submits
	 * under the frontend's queue lock with this slot's fence, and calls
	 * set_image.
	 *
	 * Returns whether an image was actually handed over. The caller MUST pass
	 * RETRO_HW_FRAME_BUFFER_VALID to retro_video_refresh only when it did:
	 * a frontend that was told a hardware frame is valid without a preceding
	 * set_image has no image to show, and retro-trainer's own video_refresh
	 * unwraps that missing image (`self.set_image.expect("set image")`). NULL
	 * -- libretro's "dupe the last frame" -- is the correct thing to send after
	 * a false, and this frontend returns immediately on it.
	 */
	bool	End();

	// -------------------------------------------------------------- targets --

	/*
	 * CreateScreen(width, height):
	 *
	 * The Vulkan stand-in for libretro's GL default framebuffer: one colour
	 * attachment plus D32_SFLOAT_S8_UINT, at the size the core reports to the
	 * frontend, plus one presentation image per frame slot.
	 */
	bool			CreateScreen(uint32_t width, uint32_t height);
	RenderTarget	*Screen()			{ return m_screen; }

	RenderTarget	*CreateTarget(uint32_t width, uint32_t height, uint32_t colourCount, bool depthStencil);
	void			DestroyTarget(RenderTarget *target);

	/*
	 * BindTarget(target):
	 *
	 * glBindFramebuffer. Ends the open render pass (transitioning the target it
	 * leaves to SHADER_READ_ONLY_OPTIMAL, so the next pass may sample it),
	 * transitions the new target's images back to attachment layouts and begins
	 * its pass with LOAD_OP_LOAD on everything. nullptr means the screen.
	 */
	void			BindTarget(RenderTarget *target);
	void			BindScreen()		{ BindTarget(m_screen); }
	RenderTarget	*CurrentTarget()	{ return m_current; }

	/* Ends the open render pass, if any. Required before any copy or blit. */
	void			EndPass();

	// ------------------------------------------------------- GL-shaped verbs --

	/* glClear, honouring the current scissor exactly as GL does. */
	void	ClearAttachments(bool colour, bool depth, bool stencil, const float rgba[4], float depthValue, uint32_t stencilValue);

	/* glViewport / glScissor / glDisable(GL_SCISSOR_TEST). y counts from row 0 = GL's bottom. */
	void	SetViewport(int x, int y, int width, int height);
	/*
	 * glScissor, and like glScissor it only sets the box: whether the test is
	 * on is SetScissorOff/SetScissorOn's business, so setting a box while the
	 * test is off records it for the next SetScissorOn rather than taking
	 * effect immediately.
	 */
	void	SetScissor(int x, int y, int width, int height);
	/*
	 * glDisable(GL_SCISSOR_TEST) / glEnable(GL_SCISSOR_TEST). Vulkan has no
	 * switch, so "off" is a scissor covering the whole target -- and, exactly
	 * like GL, turning it back on restores the box that was set before.
	 */
	void	SetScissorOff();
	void	SetScissorOn();

	/* glBindBuffer(GL_ARRAY_BUFFER) + glVertexAttribPointer's buffer half. */
	void	BindVertexBuffer(VkBuffer buffer, VkDeviceSize offset);

	/*
	 * The descriptor half of glUseProgram. This class keeps no record of the
	 * bound set: what makes it safe to bind before Draw (which binds the
	 * pipeline) is Vulkan's own rule that binding a pipeline whose layout is
	 * compatible leaves descriptor bindings alone.
	 */
	void	BindDescriptorSet(VkPipelineLayout layout, VkDescriptorSet set, uint32_t dynamicCount, const uint32_t *dynamicOffsets);

	/* glDrawArrays. Binds the pipeline if it changed and re-emits the dynamic state. */
	void	Draw(VkPipeline pipeline, uint32_t first, uint32_t count);

	/*
	 * The whole of what CRender2D and SuperAA do once their target is bound:
	 * allocate a set from this frame's pool, point its one binding at `view`,
	 * and draw the four-vertex strip the shader builds from gl_VertexIndex.
	 * Declared here rather than in VulkanShader because the descriptor pool it
	 * allocates from is the frame's.
	 */
	void	DrawTexturedQuad(const struct ShaderProgram &program, VkImageView view, VkSampler sampler);

	/*
	 * The same, for a shader whose set binds more than one image -- bindings 0
	 * to count-1, in order. R3DFrameBuffers' transparency composite samples two.
	 */
	void	DrawTexturedQuad(const struct ShaderProgram &program, const VkImageView *views, uint32_t count, VkSampler sampler);

	/* The three dynamic stencil values R3DShader drives per mesh. */
	void	SetStencil(uint32_t reference, uint32_t compareMask, uint32_t writeMask);

	void	PushConstants(VkPipelineLayout layout, VkShaderStageFlags stages, uint32_t offset, uint32_t size, const void *data);

	// ------------------------------------------------------------- resources --

	bool	CreateImage(Image &out, uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImageCreateFlags flags = 0);
	void	DestroyImage(Image &image);
	void	TransitionImage(Image &image, VkImageLayout newLayout);

	bool	CreateBuffer(Buffer &out, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags);
	void	DestroyBuffer(Buffer &buffer);

	VkSampler	Sampler(bool linear) const { return linear ? m_samplerLinear : m_samplerNearest; }

	/*
	 * glTexSubImage2D. rowLengthTexels is glPixelStorei(GL_UNPACK_ROW_LENGTH)
	 * -- 0 meaning "tight". Ends the open render pass, stages the bytes through
	 * this frame's staging ring, copies, and leaves the image in
	 * SHADER_READ_ONLY_OPTIMAL.
	 */
	void	UploadImage(Image &image, uint32_t level, int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t rowLengthTexels, const void *src, size_t srcBytes);

	/* glBufferSubData, through the same staging ring. */
	void	UploadBuffer(Buffer &buffer, VkDeviceSize offset, VkDeviceSize size, const void *src);

	/* glBlitFramebuffer(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT) between same-sized images. */
	void	CopyImage(Image &src, Image &dst, VkImageAspectFlags aspects);

	/*
	 * glReadPixels of a single texel, recorded rather than performed: the value
	 * is in dst once this frame's fence has signalled, which is what the
	 * caller's next Begin() guarantees.
	 */
	void	ReadbackToBuffer(Image &image, VkImageAspectFlags aspect, int32_t x, int32_t y, Buffer &dst, VkDeviceSize dstOffset);

	/* glUniform*: copy a block into this frame's uniform ring and bind by dynamic offset. */
	UniformSlice	AppendUniforms(const void *data, size_t size);

	/*
	 * The buffer AppendUniforms slices out of, for a descriptor set that binds
	 * a block by dynamic offset: the set names the buffer, and only the offset
	 * changes per draw. A slice that comes back naming a DIFFERENT buffer
	 * overflowed into a dedicated one and is not reachable through that set.
	 */
	VkBuffer		UniformRingBuffer() const;

	// ----------------------------------------------------------- descriptors --

	VkDescriptorSetLayout	CreateDescriptorSetLayout(const DescriptorBinding *bindings, size_t count);
	void					DestroyDescriptorSetLayout(VkDescriptorSetLayout layout);

	/* Allocated from this frame's pool, so it dies with the frame. */
	VkDescriptorSet			AllocateDescriptorSet(VkDescriptorSetLayout layout);
	void					UpdateDescriptorBuffer(VkDescriptorSet set, uint32_t binding, VkDescriptorType type, VkBuffer buffer, VkDeviceSize range);
	void					UpdateDescriptorImage(VkDescriptorSet set, uint32_t binding, VkImageView view, VkSampler sampler);
	void					UpdateDescriptorImages(VkDescriptorSet set, uint32_t binding, const VkImageView *views, uint32_t count, VkSampler sampler);

private:
	uint32_t	FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags flags) const;

	struct Ring
	{
		Buffer			buffer;
		VkDeviceSize	head	= 0;
	};

	struct FrameSlot
	{
		VkCommandPool		pool		= VK_NULL_HANDLE;
		VkCommandBuffer		cmd			= VK_NULL_HANDLE;
		VkFence				fence		= VK_NULL_HANDLE;
		bool				submitted	= false;
		VkDescriptorPool	descriptors	= VK_NULL_HANDLE;
		Ring				staging;
		Ring				uniforms;
		/*
		 * Staging buffers made for a single request that did not fit the ring
		 * (a full RAM vertex upload dwarfs it). They are freed when this slot
		 * comes round again, which is exactly when its fence has signalled.
		 */
		std::vector<Buffer>	oversized;
		Image				present;
		VkImageViewCreateInfo presentViewInfo = {};
	};

	bool	LoadFunctions(PFN_vkGetInstanceProcAddr gipa, PFN_vkGetDeviceProcAddr gdpa);
	bool	CreateSlots();
	void	DestroySlots();
	void	DestroyScreen();
	bool	RingAppend(Ring &ring, const void *data, VkDeviceSize size, VkDeviceSize alignment, VkBuffer *buffer, VkDeviceSize *offset);
	void	EmitDynamicState();
	void	EmitStencil();

	const retro_hw_render_interface_vulkan	*m_iface = nullptr;
	Api				m_vk;
	VkInstance		m_instance	= VK_NULL_HANDLE;
	VkPhysicalDevice m_gpu		= VK_NULL_HANDLE;
	VkDevice		m_device	= VK_NULL_HANDLE;
	VkQueue			m_queue		= VK_NULL_HANDLE;
	uint32_t		m_queueFamily = 0;
	VkPhysicalDeviceMemoryProperties	m_memoryProperties = {};
	VkPhysicalDeviceProperties			m_deviceProperties = {};
	bool			m_ready		= false;

	std::vector<FrameSlot>	m_slots;
	uint32_t				m_slot		= 0;
	bool					m_frameOpen	= false;

	RenderTarget	*m_screen	= nullptr;
	RenderTarget	*m_current	= nullptr;
	bool			m_passOpen	= false;

	VkSampler		m_samplerNearest = VK_NULL_HANDLE;
	VkSampler		m_samplerLinear	 = VK_NULL_HANDLE;

	VkPipeline		m_boundPipeline	= VK_NULL_HANDLE;
	VkViewport		m_viewport		= {};
	VkRect2D		m_scissor		= {};
	VkRect2D		m_savedScissor	= {};
	bool			m_scissorOff	= true;
	uint32_t		m_stencilRef	= 0;
	uint32_t		m_stencilCompareMask = 0xFF;
	uint32_t		m_stencilWriteMask	 = 0xFF;

	retro_vulkan_image	m_presentImage = {};	// must outlive the retro_video_refresh call
};

} // namespace Vulkan

#endif	// INCLUDED_VULKAN_VULKANCONTEXT_H
