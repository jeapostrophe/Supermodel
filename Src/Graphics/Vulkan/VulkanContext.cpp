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

#include "VulkanContext.h"
#include "VulkanShader.h"

#include <cstring>

#include "Supermodel.h"

namespace Vulkan {

static const VkDeviceSize	STAGING_RING_BYTES	= 8u * 1024u * 1024u;
static const VkDeviceSize	UNIFORM_RING_BYTES	= 1u * 1024u * 1024u;
static const uint32_t		DESCRIPTOR_SETS_PER_FRAME = 1024;

VulkanContext &VulkanContext::Get()
{
	static VulkanContext context;
	return context;
}

/******************************************************************************
 Function loading
******************************************************************************/

bool VulkanContext::LoadFunctions(PFN_vkGetInstanceProcAddr gipa, PFN_vkGetDeviceProcAddr gdpa)
{
#define VULKAN_LOAD_INSTANCE_FUNCTION(name)											\
	m_vk.name = (PFN_##name)gipa(m_instance, #name);								\
	if (m_vk.name == nullptr)														\
	{																				\
		ErrorLog("Vulkan: the frontend's instance does not provide %s.\n", #name);	\
		return false;																\
	}
	VULKAN_INSTANCE_FUNCTIONS(VULKAN_LOAD_INSTANCE_FUNCTION)
#undef VULKAN_LOAD_INSTANCE_FUNCTION

	/*
	 * The interface's get_device_proc_addr is allowed to be null and really is
	 * null in retro-trainer (measured 2026-09-01: `get_device_proc_addr: None`
	 * in src/vulkan.rs), so fall back to the one just loaded from the instance.
	 */
	if (gdpa == nullptr)
		gdpa = m_vk.vkGetDeviceProcAddr;

#define VULKAN_LOAD_DEVICE_FUNCTION(name)											\
	m_vk.name = (PFN_##name)gdpa(m_device, #name);									\
	if (m_vk.name == nullptr)														\
	{																				\
		ErrorLog("Vulkan: the frontend's device does not provide %s.\n", #name);		\
		return false;																\
	}
	VULKAN_DEVICE_FUNCTIONS(VULKAN_LOAD_DEVICE_FUNCTION)
#undef VULKAN_LOAD_DEVICE_FUNCTION

	return true;
}

/******************************************************************************
 Initialization and teardown
******************************************************************************/

bool VulkanContext::Init(const retro_hw_render_interface_vulkan *iface)
{
	if (m_ready)
		Shutdown();

	if (iface == nullptr || iface->get_instance_proc_addr == nullptr)
	{
		ErrorLog("Vulkan: the frontend supplied no get_instance_proc_addr.\n");
		return false;
	}

	m_iface			= iface;
	m_instance		= iface->instance;
	m_gpu			= iface->gpu;
	m_device		= iface->device;
	m_queue			= iface->queue;
	m_queueFamily	= iface->queue_index;

	if (m_device == VK_NULL_HANDLE || m_queue == VK_NULL_HANDLE)
	{
		ErrorLog("Vulkan: the frontend supplied no device or queue.\n");
		m_iface = nullptr;
		return false;
	}

	if (!LoadFunctions(iface->get_instance_proc_addr, iface->get_device_proc_addr))
	{
		m_iface = nullptr;
		return false;
	}

	m_vk.vkGetPhysicalDeviceMemoryProperties(m_gpu, &m_memoryProperties);
	m_vk.vkGetPhysicalDeviceProperties(m_gpu, &m_deviceProperties);

	InfoLog("Vulkan: %s (API %u.%u.%u), queue family %u.\n",
		m_deviceProperties.deviceName,
		VK_VERSION_MAJOR(m_deviceProperties.apiVersion),
		VK_VERSION_MINOR(m_deviceProperties.apiVersion),
		VK_VERSION_PATCH(m_deviceProperties.apiVersion),
		m_queueFamily);

	if (!CreateSlots())
	{
		Shutdown();
		return false;
	}

	VkSamplerCreateInfo sampler = {};
	sampler.sType			= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.magFilter		= VK_FILTER_NEAREST;
	sampler.minFilter		= VK_FILTER_NEAREST;
	sampler.mipmapMode		= VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sampler.addressModeU	= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV	= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeW	= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.maxLod			= VK_LOD_CLAMP_NONE;
	sampler.borderColor		= VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	if (m_vk.vkCreateSampler(m_device, &sampler, nullptr, &m_samplerNearest) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: could not create the nearest sampler.\n");
		Shutdown();
		return false;
	}

	sampler.magFilter	= VK_FILTER_LINEAR;
	sampler.minFilter	= VK_FILTER_LINEAR;
	sampler.mipmapMode	= VK_SAMPLER_MIPMAP_MODE_LINEAR;
	if (m_vk.vkCreateSampler(m_device, &sampler, nullptr, &m_samplerLinear) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: could not create the linear sampler.\n");
		Shutdown();
		return false;
	}

	m_ready = true;
	return true;
}

bool VulkanContext::CreateSlots()
{
	uint32_t mask = 1;
	if (m_iface->get_sync_index_mask != nullptr)
		mask = m_iface->get_sync_index_mask(m_iface->handle);
	if (mask == 0)
		mask = 1;

	uint32_t count = 0;
	for (uint32_t bit = 0; bit < 32; bit++)
	{
		if (mask & (1u << bit))
			count = bit + 1;			// slots are indexed by get_sync_index, so size to the highest bit
	}

	m_slots.resize(count);
	DebugLog("Vulkan: sync index mask 0x%X, %u frame slots.\n", mask, count);

	for (FrameSlot &slot : m_slots)
	{
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType				= VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex	= m_queueFamily;
		if (m_vk.vkCreateCommandPool(m_device, &poolInfo, nullptr, &slot.pool) != VK_SUCCESS)
		{
			ErrorLog("Vulkan: could not create a command pool.\n");
			return false;
		}

		VkCommandBufferAllocateInfo cmdInfo = {};
		cmdInfo.sType				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdInfo.commandPool			= slot.pool;
		cmdInfo.level				= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdInfo.commandBufferCount	= 1;
		if (m_vk.vkAllocateCommandBuffers(m_device, &cmdInfo, &slot.cmd) != VK_SUCCESS)
		{
			ErrorLog("Vulkan: could not allocate a command buffer.\n");
			return false;
		}

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		if (m_vk.vkCreateFence(m_device, &fenceInfo, nullptr, &slot.fence) != VK_SUCCESS)
		{
			ErrorLog("Vulkan: could not create a fence.\n");
			return false;
		}

		VkDescriptorPoolSize sizes[2] = {};
		sizes[0].type			= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		sizes[0].descriptorCount = DESCRIPTOR_SETS_PER_FRAME;
		sizes[1].type			= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sizes[1].descriptorCount = DESCRIPTOR_SETS_PER_FRAME * 4;

		VkDescriptorPoolCreateInfo descInfo = {};
		descInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descInfo.maxSets		= DESCRIPTOR_SETS_PER_FRAME;
		descInfo.poolSizeCount	= 2;
		descInfo.pPoolSizes		= sizes;
		if (m_vk.vkCreateDescriptorPool(m_device, &descInfo, nullptr, &slot.descriptors) != VK_SUCCESS)
		{
			ErrorLog("Vulkan: could not create a descriptor pool.\n");
			return false;
		}

		if (!CreateBuffer(slot.staging.buffer, STAGING_RING_BYTES, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			return false;
		if (!CreateBuffer(slot.uniforms.buffer, UNIFORM_RING_BYTES, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			return false;
	}

	return true;
}

void VulkanContext::DestroySlots()
{
	for (FrameSlot &slot : m_slots)
	{
		DestroyBuffer(slot.staging.buffer);
		DestroyBuffer(slot.uniforms.buffer);
		for (Buffer &buffer : slot.oversized)
			DestroyBuffer(buffer);
		slot.oversized.clear();
		DestroyImage(slot.present);

		if (slot.descriptors != VK_NULL_HANDLE)
			m_vk.vkDestroyDescriptorPool(m_device, slot.descriptors, nullptr);
		if (slot.fence != VK_NULL_HANDLE)
			m_vk.vkDestroyFence(m_device, slot.fence, nullptr);
		if (slot.pool != VK_NULL_HANDLE)
			m_vk.vkDestroyCommandPool(m_device, slot.pool, nullptr);	// frees its command buffers

		slot = FrameSlot();
	}
	m_slots.clear();
}

void VulkanContext::Shutdown()
{
	if (m_device != VK_NULL_HANDLE && m_vk.vkDeviceWaitIdle != nullptr)
		m_vk.vkDeviceWaitIdle(m_device);

	DestroyScreen();
	DestroySlots();

	if (m_samplerNearest != VK_NULL_HANDLE)
		m_vk.vkDestroySampler(m_device, m_samplerNearest, nullptr);
	if (m_samplerLinear != VK_NULL_HANDLE)
		m_vk.vkDestroySampler(m_device, m_samplerLinear, nullptr);
	m_samplerNearest	= VK_NULL_HANDLE;
	m_samplerLinear		= VK_NULL_HANDLE;

	// The VkDevice, VkQueue, VkPhysicalDevice and VkInstance belong to the
	// frontend. Forget them; never destroy them.
	m_iface			= nullptr;
	m_instance		= VK_NULL_HANDLE;
	m_gpu			= VK_NULL_HANDLE;
	m_device		= VK_NULL_HANDLE;
	m_queue			= VK_NULL_HANDLE;
	m_current		= nullptr;
	m_passOpen		= false;
	m_frameOpen		= false;
	m_boundPipeline	= VK_NULL_HANDLE;
	m_ready			= false;
	m_vk			= Api();
}

/******************************************************************************
 Memory, images and buffers
******************************************************************************/

uint32_t VulkanContext::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags flags) const
{
	for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; i++)
	{
		if ((typeBits & (1u << i)) && (m_memoryProperties.memoryTypes[i].propertyFlags & flags) == flags)
			return i;
	}
	ErrorLog("Vulkan: no memory type satisfies bits 0x%X with properties 0x%X.\n", typeBits, (unsigned)flags);
	return UINT32_MAX;
}

bool VulkanContext::CreateImage(Image &out, uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImageCreateFlags flags)
{
	DestroyImage(out);

	// Shutdown() clears the function table, so without this the class's "a call
	// after Shutdown is a no-op" promise would be a jump through a null pointer.
	if (m_device == VK_NULL_HANDLE)
		return false;

	VkImageCreateInfo info = {};
	info.sType			= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.flags			= flags;
	info.imageType		= VK_IMAGE_TYPE_2D;
	info.format			= format;
	info.extent.width	= width;
	info.extent.height	= height;
	info.extent.depth	= 1;
	info.mipLevels		= levels;
	info.arrayLayers	= 1;
	info.samples		= VK_SAMPLE_COUNT_1_BIT;
	info.tiling			= VK_IMAGE_TILING_OPTIMAL;
	info.usage			= usage;
	info.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;
	info.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;

	if (m_vk.vkCreateImage(m_device, &info, nullptr, &out.image) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: could not create a %ux%u image.\n", width, height);
		return false;
	}

	VkMemoryRequirements requirements = {};
	m_vk.vkGetImageMemoryRequirements(m_device, out.image, &requirements);

	const uint32_t type = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (type == UINT32_MAX)
	{
		DestroyImage(out);
		return false;
	}

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType				= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize	= requirements.size;
	allocInfo.memoryTypeIndex	= type;
	if (m_vk.vkAllocateMemory(m_device, &allocInfo, nullptr, &out.memory) != VK_SUCCESS ||
		m_vk.vkBindImageMemory(m_device, out.image, out.memory, 0) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: could not back a %ux%u image with memory.\n", width, height);
		DestroyImage(out);
		return false;
	}

	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType		= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image		= out.image;
	viewInfo.viewType	= VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format		= format;
	viewInfo.subresourceRange.aspectMask		= aspect;
	viewInfo.subresourceRange.levelCount		= levels;
	viewInfo.subresourceRange.layerCount		= 1;
	if (m_vk.vkCreateImageView(m_device, &viewInfo, nullptr, &out.view) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: could not create an image view.\n");
		DestroyImage(out);
		return false;
	}

	out.format	= format;
	out.aspect	= aspect;
	out.layout	= VK_IMAGE_LAYOUT_UNDEFINED;
	out.width	= width;
	out.height	= height;
	out.levels	= levels;
	return true;
}

void VulkanContext::DestroyImage(Image &image)
{
	if (m_device == VK_NULL_HANDLE)
	{
		image = Image();
		return;
	}
	if (image.view != VK_NULL_HANDLE)
		m_vk.vkDestroyImageView(m_device, image.view, nullptr);
	if (image.image != VK_NULL_HANDLE)
		m_vk.vkDestroyImage(m_device, image.image, nullptr);
	if (image.memory != VK_NULL_HANDLE)
		m_vk.vkFreeMemory(m_device, image.memory, nullptr);
	image = Image();
}

bool VulkanContext::CreateBuffer(Buffer &out, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags)
{
	DestroyBuffer(out);

	if (m_device == VK_NULL_HANDLE)
		return false;

	VkBufferCreateInfo info = {};
	info.sType			= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.size			= size;
	info.usage			= usage;
	info.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;
	if (m_vk.vkCreateBuffer(m_device, &info, nullptr, &out.buffer) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: could not create a %llu byte buffer.\n", (unsigned long long)size);
		return false;
	}

	VkMemoryRequirements requirements = {};
	m_vk.vkGetBufferMemoryRequirements(m_device, out.buffer, &requirements);

	const uint32_t type = FindMemoryType(requirements.memoryTypeBits, memoryFlags);
	if (type == UINT32_MAX)
	{
		DestroyBuffer(out);
		return false;
	}

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType				= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize	= requirements.size;
	allocInfo.memoryTypeIndex	= type;
	if (m_vk.vkAllocateMemory(m_device, &allocInfo, nullptr, &out.memory) != VK_SUCCESS ||
		m_vk.vkBindBufferMemory(m_device, out.buffer, out.memory, 0) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: could not back a %llu byte buffer with memory.\n", (unsigned long long)size);
		DestroyBuffer(out);
		return false;
	}

	if (memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		if (m_vk.vkMapMemory(m_device, out.memory, 0, VK_WHOLE_SIZE, 0, &out.mapped) != VK_SUCCESS)
		{
			ErrorLog("Vulkan: could not map a host-visible buffer.\n");
			DestroyBuffer(out);
			return false;
		}
	}

	out.size = size;
	return true;
}

void VulkanContext::DestroyBuffer(Buffer &buffer)
{
	if (m_device == VK_NULL_HANDLE)
	{
		buffer = Buffer();
		return;
	}
	if (buffer.mapped != nullptr && buffer.memory != VK_NULL_HANDLE)
		m_vk.vkUnmapMemory(m_device, buffer.memory);
	if (buffer.buffer != VK_NULL_HANDLE)
		m_vk.vkDestroyBuffer(m_device, buffer.buffer, nullptr);
	if (buffer.memory != VK_NULL_HANDLE)
		m_vk.vkFreeMemory(m_device, buffer.memory, nullptr);
	buffer = Buffer();
}

/*
 * The access mask that goes with a layout. Paired with an ALL_COMMANDS stage
 * mask on both sides this is coarser than it needs to be, but the port issues a
 * handful of barriers per frame -- correctness is worth far more here than the
 * pipelining a tighter mask would buy.
 */
static VkAccessFlags AccessForLayout(VkImageLayout layout)
{
	switch (layout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:						return 0;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:		return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:	return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:		return VK_ACCESS_SHADER_READ_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:			return VK_ACCESS_TRANSFER_READ_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:			return VK_ACCESS_TRANSFER_WRITE_BIT;
	default:											return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	}
}

void VulkanContext::TransitionImage(Image &image, VkImageLayout newLayout)
{
	// Outside a frame there is no command buffer to record the barrier into, so
	// the image keeps the layout it has and the transition happens the next
	// time it is bound or sampled inside one.
	if (!m_frameOpen || !image.Valid() || image.layout == newLayout)
		return;

	VkImageMemoryBarrier barrier = {};
	barrier.sType				= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask		= AccessForLayout(image.layout);
	barrier.dstAccessMask		= AccessForLayout(newLayout);
	barrier.oldLayout			= image.layout;
	barrier.newLayout			= newLayout;
	barrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.image				= image.image;
	barrier.subresourceRange.aspectMask	= image.aspect;
	barrier.subresourceRange.levelCount	= image.levels;
	barrier.subresourceRange.layerCount	= 1;

	m_vk.vkCmdPipelineBarrier(Cmd(),
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		0, 0, nullptr, 0, nullptr, 1, &barrier);

	image.layout = newLayout;
}

/******************************************************************************
 Render targets
******************************************************************************/

RenderTarget *VulkanContext::CreateTarget(uint32_t width, uint32_t height, uint32_t colourCount, bool depthStencil)
{
	if (!m_ready || width == 0 || height == 0 || colourCount == 0)
		return nullptr;

	RenderTarget *target = new RenderTarget();
	target->width	= width;
	target->height	= height;
	target->colour.resize(colourCount);

	for (uint32_t i = 0; i < colourCount; i++)
	{
		if (!CreateImage(target->colour[i], width, height, 1, VK_FORMAT_R8G8B8A8_UNORM,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT))
		{
			DestroyTarget(target);
			return nullptr;
		}
	}

	if (depthStencil)
	{
		if (!CreateImage(target->depth, width, height, 1, VK_FORMAT_D32_SFLOAT_S8_UINT,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
		{
			DestroyTarget(target);
			return nullptr;
		}
	}

	const uint32_t attachmentCount = colourCount + (depthStencil ? 1 : 0);
	std::vector<VkAttachmentDescription>	attachments(attachmentCount);
	std::vector<VkAttachmentReference>		colourRefs(colourCount);

	for (uint32_t i = 0; i < colourCount; i++)
	{
		attachments[i].format			= VK_FORMAT_R8G8B8A8_UNORM;
		attachments[i].samples			= VK_SAMPLE_COUNT_1_BIT;
		attachments[i].loadOp			= VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[i].storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
		attachments[i].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[i].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[i].initialLayout	= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[i].finalLayout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		colourRefs[i].attachment	= i;
		colourRefs[i].layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	VkAttachmentReference depthRef = {};
	if (depthStencil)
	{
		const uint32_t i = colourCount;
		attachments[i].format			= VK_FORMAT_D32_SFLOAT_S8_UINT;
		attachments[i].samples			= VK_SAMPLE_COUNT_1_BIT;
		attachments[i].loadOp			= VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[i].storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
		attachments[i].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[i].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_STORE;
		attachments[i].initialLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[i].finalLayout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		depthRef.attachment	= i;
		depthRef.layout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount	= colourCount;
	subpass.pColorAttachments		= colourRefs.data();
	subpass.pDepthStencilAttachment	= depthStencil ? &depthRef : nullptr;

	/*
	 * The port brackets every pass with explicit barriers, so these two
	 * dependencies only have to be no weaker than those: ALL_COMMANDS both ways
	 * says "whatever happened before this pass is visible to it, and whatever
	 * the pass wrote is visible after".
	 */
	VkSubpassDependency dependencies[2] = {};
	dependencies[0].srcSubpass		= VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass		= 0;
	dependencies[0].srcStageMask	= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	dependencies[0].dstStageMask	= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	dependencies[0].srcAccessMask	= VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	dependencies[0].dstAccessMask	= VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	dependencies[1] = dependencies[0];
	dependencies[1].srcSubpass		= 0;
	dependencies[1].dstSubpass		= VK_SUBPASS_EXTERNAL;

	VkRenderPassCreateInfo passInfo = {};
	passInfo.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	passInfo.attachmentCount	= attachmentCount;
	passInfo.pAttachments		= attachments.data();
	passInfo.subpassCount		= 1;
	passInfo.pSubpasses			= &subpass;
	passInfo.dependencyCount	= 2;
	passInfo.pDependencies		= dependencies;

	if (m_vk.vkCreateRenderPass(m_device, &passInfo, nullptr, &target->pass) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: could not create a render pass.\n");
		DestroyTarget(target);
		return nullptr;
	}

	std::vector<VkImageView> views;
	views.reserve(attachmentCount);
	for (uint32_t i = 0; i < colourCount; i++)
		views.push_back(target->colour[i].view);
	if (depthStencil)
		views.push_back(target->depth.view);

	VkFramebufferCreateInfo fbInfo = {};
	fbInfo.sType			= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbInfo.renderPass		= target->pass;
	fbInfo.attachmentCount	= attachmentCount;
	fbInfo.pAttachments		= views.data();
	fbInfo.width			= width;
	fbInfo.height			= height;
	fbInfo.layers			= 1;

	if (m_vk.vkCreateFramebuffer(m_device, &fbInfo, nullptr, &target->framebuffer) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: could not create a framebuffer.\n");
		DestroyTarget(target);
		return nullptr;
	}

	return target;
}

void VulkanContext::DestroyTarget(RenderTarget *target)
{
	if (target == nullptr)
		return;

	if (m_current == target)
	{
		EndPass();
		m_current = nullptr;
	}

	if (m_device != VK_NULL_HANDLE)
	{
		if (target->framebuffer != VK_NULL_HANDLE)
			m_vk.vkDestroyFramebuffer(m_device, target->framebuffer, nullptr);
		if (target->pass != VK_NULL_HANDLE)
			m_vk.vkDestroyRenderPass(m_device, target->pass, nullptr);
		for (Image &image : target->colour)
			DestroyImage(image);
		DestroyImage(target->depth);
	}

	delete target;
}

void VulkanContext::EndPass()
{
	if (!m_passOpen)
		return;

	m_vk.vkCmdEndRenderPass(Cmd());
	m_passOpen		= false;
	m_boundPipeline	= VK_NULL_HANDLE;

	/*
	 * Whatever was just rendered is normally read next as a texture (SuperAA
	 * samples the AA target, R3DFrameBuffers samples its three colour layers).
	 * Doing the transition here, rather than in the renderer, is what lets the
	 * renderers stay a line-for-line port of the GL originals.
	 */
	if (m_current != nullptr)
	{
		for (Image &image : m_current->colour)
			TransitionImage(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
}

void VulkanContext::BindTarget(RenderTarget *target)
{
	if (!m_frameOpen)
		return;
	if (target == nullptr)
		target = m_screen;
	if (target == nullptr)
		return;
	if (m_passOpen && m_current == target)
		return;

	EndPass();

	for (Image &image : target->colour)
		TransitionImage(image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	if (target->HasDepth())
		TransitionImage(target->depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	VkRenderPassBeginInfo begin = {};
	begin.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	begin.renderPass				= target->pass;
	begin.framebuffer				= target->framebuffer;
	begin.renderArea.extent.width	= target->width;
	begin.renderArea.extent.height	= target->height;

	m_vk.vkCmdBeginRenderPass(Cmd(), &begin, VK_SUBPASS_CONTENTS_INLINE);

	m_current		= target;
	m_passOpen		= true;
	m_boundPipeline	= VK_NULL_HANDLE;

	if (m_scissorOff)
		SetScissorOff();				// re-clamp "no scissor" to the new target's extent
	EmitDynamicState();
}

/******************************************************************************
 The screen target and presentation
******************************************************************************/

bool VulkanContext::CreateScreen(uint32_t width, uint32_t height)
{
	if (!m_ready)
		return false;

	if (m_screen != nullptr && m_screen->width == width && m_screen->height == height)
		return true;

	m_vk.vkDeviceWaitIdle(m_device);
	DestroyScreen();

	m_screen = CreateTarget(width, height, 1, true);
	if (m_screen == nullptr)
		return false;

	for (FrameSlot &slot : m_slots)
	{
		/*
		 * libretro requires TRANSFER_SRC and SAMPLED on the image handed to
		 * set_image, and MUTABLE_FORMAT so the frontend may reinterpret an
		 * 8-bit format as sRGB or not as it sees fit. TRANSFER_DST is ours: the
		 * flipped blit in End() writes it.
		 */
		if (!CreateImage(slot.present, width, height, 1, VK_FORMAT_R8G8B8A8_UNORM,
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT))
		{
			DestroyScreen();
			return false;
		}

		/*
		 * The frontend keeps the whole VkImageViewCreateInfo, not just the view
		 * -- retro-trainer reads create_info.image to find the VkImage to blit
		 * from (measured 2026-09-01, src/vulkan.rs vulkan_set_image) -- so it
		 * has to be the real one, kept alive for as long as the image is.
		 */
		VkImageViewCreateInfo &viewInfo = slot.presentViewInfo;
		viewInfo = VkImageViewCreateInfo();
		viewInfo.sType		= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image		= slot.present.image;
		viewInfo.viewType	= VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format		= VK_FORMAT_R8G8B8A8_UNORM;
		viewInfo.subresourceRange.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.levelCount	= 1;
		viewInfo.subresourceRange.layerCount	= 1;
	}

	DebugLog("Vulkan: screen target and %u presentation images at %ux%u.\n", (unsigned)m_slots.size(), width, height);
	return true;
}

void VulkanContext::DestroyScreen()
{
	for (FrameSlot &slot : m_slots)
	{
		DestroyImage(slot.present);
		slot.presentViewInfo = VkImageViewCreateInfo();
	}
	if (m_screen != nullptr)
	{
		DestroyTarget(m_screen);
		m_screen = nullptr;
	}
	m_presentImage = retro_vulkan_image();
}

/******************************************************************************
 Frame
******************************************************************************/

bool VulkanContext::Begin(uint32_t width, uint32_t height)
{
	if (!m_ready || m_frameOpen)
		return false;

	uint32_t index = 0;
	if (m_iface->get_sync_index != nullptr)
		index = m_iface->get_sync_index(m_iface->handle);
	if (index >= m_slots.size())
		index = 0;
	m_slot = index;

	FrameSlot &slot = m_slots[m_slot];

	if (slot.submitted)
	{
		m_vk.vkWaitForFences(m_device, 1, &slot.fence, VK_TRUE, UINT64_MAX);
		m_vk.vkResetFences(m_device, 1, &slot.fence);
		slot.submitted = false;
	}

	/*
	 * Everything below is now provably idle: this slot's fence has signalled,
	 * so nothing the GPU still reads lives in its pools or rings.
	 */
	m_vk.vkResetCommandPool(m_device, slot.pool, 0);
	m_vk.vkResetDescriptorPool(m_device, slot.descriptors, 0);
	slot.staging.head	= 0;
	slot.uniforms.head	= 0;
	for (Buffer &buffer : slot.oversized)
		DestroyBuffer(buffer);
	slot.oversized.clear();

	if (!CreateScreen(width, height))		// returns early if the size is unchanged
		return false;

	VkCommandBufferBeginInfo begin = {};
	begin.sType	= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.flags	= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (m_vk.vkBeginCommandBuffer(slot.cmd, &begin) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: could not begin the frame's command buffer.\n");
		return false;
	}

	m_frameOpen		= true;
	m_passOpen		= false;
	m_current		= nullptr;
	m_boundPipeline	= VK_NULL_HANDLE;
	return true;
}

bool VulkanContext::End()
{
	if (!m_frameOpen)
		return false;

	FrameSlot &slot = m_slots[m_slot];

	EndPass();

	if (m_screen != nullptr && slot.present.Valid())
	{
		Image &source = m_screen->colour[0];
		TransitionImage(source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		TransitionImage(slot.present, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		/*
		 * THE ONE FLIP. Everything upstream renders with row 0 as GL's bottom
		 * row; the frontends take a Vulkan core's image top-down. Inverting the
		 * source y offsets here is what reconciles them, and it is the only
		 * place in the port where the two conventions meet.
		 */
		VkImageBlit blit = {};
		blit.srcSubresource.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.layerCount	= 1;
		blit.dstSubresource.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.layerCount	= 1;
		blit.srcOffsets[0]	= { 0, (int32_t)m_screen->height, 0 };
		blit.srcOffsets[1]	= { (int32_t)m_screen->width, 0, 1 };
		blit.dstOffsets[0]	= { 0, 0, 0 };
		blit.dstOffsets[1]	= { (int32_t)slot.present.width, (int32_t)slot.present.height, 1 };

		m_vk.vkCmdBlitImage(slot.cmd,
			source.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			slot.present.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit, VK_FILTER_NEAREST);

		TransitionImage(slot.present, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	if (m_vk.vkEndCommandBuffer(slot.cmd) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: could not end the frame's command buffer.\n");
		m_frameOpen = false;
		return false;
	}

	VkSubmitInfo submit = {};
	submit.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount	= 1;
	submit.pCommandBuffers		= &slot.cmd;

	if (m_iface->lock_queue != nullptr)
		m_iface->lock_queue(m_iface->handle);
	const VkResult result = m_vk.vkQueueSubmit(m_queue, 1, &submit, slot.fence);
	if (m_iface->unlock_queue != nullptr)
		m_iface->unlock_queue(m_iface->handle);

	if (result != VK_SUCCESS)
	{
		ErrorLog("Vulkan: vkQueueSubmit failed (%d).\n", (int)result);
		m_frameOpen = false;
		return false;
	}
	slot.submitted = true;

	bool presented = false;
	if (m_iface->set_image != nullptr && slot.present.Valid())
	{
		m_presentImage.image_view	= slot.present.view;
		m_presentImage.image_layout	= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		m_presentImage.create_info	= slot.presentViewInfo;
		// num_semaphores must be 0 and src_queue_family must be the frontend's
		// own queue family, or retro-trainer drops the frame.
		m_iface->set_image(m_iface->handle, &m_presentImage, 0, nullptr, m_queueFamily);
		presented = true;
	}

	m_frameOpen		= false;
	m_current		= nullptr;
	m_boundPipeline	= VK_NULL_HANDLE;
	return presented;
}

/******************************************************************************
 GL-shaped verbs
******************************************************************************/

void VulkanContext::EmitDynamicState()
{
	if (!m_passOpen)
		return;
	m_vk.vkCmdSetViewport(Cmd(), 0, 1, &m_viewport);
	m_vk.vkCmdSetScissor(Cmd(), 0, 1, &m_scissor);
	EmitStencil();
}

/*
 * Every pipeline this port builds declares the three stencil values dynamic
 * (see VulkanShader::CreatePipeline), so all three are always re-stated --
 * leaving one unset would make a draw with a stencil pipeline invalid.
 */
void VulkanContext::EmitStencil()
{
	if (!m_passOpen)
		return;
	const VkStencilFaceFlags faces = VK_STENCIL_FRONT_AND_BACK;
	m_vk.vkCmdSetStencilReference(Cmd(), faces, m_stencilRef);
	m_vk.vkCmdSetStencilCompareMask(Cmd(), faces, m_stencilCompareMask);
	m_vk.vkCmdSetStencilWriteMask(Cmd(), faces, m_stencilWriteMask);
}

void VulkanContext::SetViewport(int x, int y, int width, int height)
{
	// y counts from row 0, which this port calls GL's bottom row -- so the
	// numbers the GL sources compute go through unchanged, with a positive
	// height and no VK_KHR_maintenance1 viewport flip.
	m_viewport.x		= (float)x;
	m_viewport.y		= (float)y;
	m_viewport.width	= (float)width;
	m_viewport.height	= (float)height;
	m_viewport.minDepth	= 0.0f;
	m_viewport.maxDepth	= 1.0f;
	if (m_passOpen)
		m_vk.vkCmdSetViewport(Cmd(), 0, 1, &m_viewport);
}

void VulkanContext::SetScissor(int x, int y, int width, int height)
{
	if (x < 0) { width += x; x = 0; }
	if (y < 0) { height += y; y = 0; }
	if (width < 0) width = 0;
	if (height < 0) height = 0;

	VkRect2D box = {};
	box.offset.x		= x;
	box.offset.y		= y;
	box.extent.width	= (uint32_t)width;
	box.extent.height	= (uint32_t)height;

	// glScissor sets the box whether or not the test is on. With the test off
	// the live rect is the whole target, so record the box for the SetScissorOn
	// that will restore it rather than applying it now.
	if (m_scissorOff)
	{
		m_savedScissor = box;
		return;
	}

	m_scissor = box;
	if (m_passOpen)
		m_vk.vkCmdSetScissor(Cmd(), 0, 1, &m_scissor);
}

void VulkanContext::SetScissorOff()
{
	// GL turns the test off; Vulkan has no such switch, so "off" is the whole
	// target. Clears go through the scissor rect too, which is what makes
	// Setup2D's "disable scissor, clear, enable scissor" behave as it does.
	const uint32_t width	= m_current != nullptr ? m_current->width  : (m_screen != nullptr ? m_screen->width  : 0);
	const uint32_t height	= m_current != nullptr ? m_current->height : (m_screen != nullptr ? m_screen->height : 0);

	if (!m_scissorOff)
		m_savedScissor = m_scissor;
	m_scissorOff			= true;
	m_scissor.offset.x		= 0;
	m_scissor.offset.y		= 0;
	m_scissor.extent.width	= width;
	m_scissor.extent.height	= height;
	if (m_passOpen)
		m_vk.vkCmdSetScissor(Cmd(), 0, 1, &m_scissor);
}

void VulkanContext::SetScissorOn()
{
	if (!m_scissorOff)
		return;
	m_scissorOff	= false;
	m_scissor		= m_savedScissor;
	if (m_passOpen)
		m_vk.vkCmdSetScissor(Cmd(), 0, 1, &m_scissor);
}

void VulkanContext::SetStencil(uint32_t reference, uint32_t compareMask, uint32_t writeMask)
{
	m_stencilRef			= reference;
	m_stencilCompareMask	= compareMask;
	m_stencilWriteMask		= writeMask;
	EmitStencil();
}

void VulkanContext::ClearAttachments(bool colour, bool depth, bool stencil, const float rgba[4], float depthValue, uint32_t stencilValue)
{
	if (!m_passOpen || m_current == nullptr)
		return;

	VkClearAttachment attachments[4] = {};	// 3 colour (R3DFrameBuffers' MRT) + depth/stencil
	uint32_t count = 0;

	if (colour)
	{
		for (uint32_t i = 0; i < m_current->colour.size() && count < 3; i++, count++)
		{
			attachments[count].aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
			attachments[count].colorAttachment	= i;
			attachments[count].clearValue.color.float32[0] = rgba != nullptr ? rgba[0] : 0.0f;
			attachments[count].clearValue.color.float32[1] = rgba != nullptr ? rgba[1] : 0.0f;
			attachments[count].clearValue.color.float32[2] = rgba != nullptr ? rgba[2] : 0.0f;
			attachments[count].clearValue.color.float32[3] = rgba != nullptr ? rgba[3] : 0.0f;
		}
	}

	if ((depth || stencil) && m_current->HasDepth())
	{
		attachments[count].aspectMask =
			(depth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) | (stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
		attachments[count].clearValue.depthStencil.depth		= depthValue;
		attachments[count].clearValue.depthStencil.stencil	= stencilValue;
		count++;
	}

	if (count == 0)
		return;

	/*
	 * GL clears honour the scissor test, so the clear rect IS the scissor --
	 * but GL also silently clips a scissor that hangs off the framebuffer,
	 * which it does whenever supersampling makes ApplyGLGeometry's numbers
	 * bigger than the screen target. Vulkan requires the clear rect to lie
	 * inside the render area, so clip it here the way GL would.
	 */
	VkClearRect rect = {};
	rect.rect		= m_scissor;
	rect.layerCount	= 1;
	if (rect.rect.offset.x + (int32_t)rect.rect.extent.width > (int32_t)m_current->width)
		rect.rect.extent.width = (uint32_t)((int32_t)m_current->width - rect.rect.offset.x);
	if (rect.rect.offset.y + (int32_t)rect.rect.extent.height > (int32_t)m_current->height)
		rect.rect.extent.height = (uint32_t)((int32_t)m_current->height - rect.rect.offset.y);
	if (rect.rect.offset.x >= (int32_t)m_current->width || rect.rect.offset.y >= (int32_t)m_current->height ||
		rect.rect.extent.width == 0 || rect.rect.extent.height == 0)
		return;

	m_vk.vkCmdClearAttachments(Cmd(), count, attachments, 1, &rect);
}

void VulkanContext::BindVertexBuffer(VkBuffer buffer, VkDeviceSize offset)
{
	if (!m_passOpen || buffer == VK_NULL_HANDLE)
		return;
	m_vk.vkCmdBindVertexBuffers(Cmd(), 0, 1, &buffer, &offset);
}

void VulkanContext::BindDescriptorSet(VkPipelineLayout layout, VkDescriptorSet set, uint32_t dynamicCount, const uint32_t *dynamicOffsets)
{
	if (!m_passOpen || set == VK_NULL_HANDLE)
		return;
	m_vk.vkCmdBindDescriptorSets(Cmd(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, dynamicCount, dynamicOffsets);
}

void VulkanContext::Draw(VkPipeline pipeline, uint32_t first, uint32_t count)
{
	if (!m_passOpen || count == 0)
		return;

	if (pipeline != VK_NULL_HANDLE && pipeline != m_boundPipeline)
	{
		m_vk.vkCmdBindPipeline(Cmd(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		m_boundPipeline = pipeline;
		EmitDynamicState();				// a new pipeline needs its dynamic state re-stated
	}

	m_vk.vkCmdDraw(Cmd(), count, 1, first, 0);
}

void VulkanContext::DrawTexturedQuad(const ShaderProgram &program, VkImageView view, VkSampler sampler)
{
	if (!m_passOpen || !program.Valid() || view == VK_NULL_HANDLE)
		return;

	VkDescriptorSet set = AllocateDescriptorSet(program.setLayout);
	if (set == VK_NULL_HANDLE)
		return;

	UpdateDescriptorImage(set, 0, view, sampler);
	BindDescriptorSet(program.pipelineLayout, set, 0, nullptr);
	Draw(program.pipeline, 0, 4);
}

void VulkanContext::PushConstants(VkPipelineLayout layout, VkShaderStageFlags stages, uint32_t offset, uint32_t size, const void *data)
{
	if (!m_frameOpen)
		return;
	m_vk.vkCmdPushConstants(Cmd(), layout, stages, offset, size, data);
}

/******************************************************************************
 Uploads, copies and readback
******************************************************************************/

bool VulkanContext::RingAppend(Ring &ring, const void *data, VkDeviceSize size, VkDeviceSize alignment, VkBuffer *buffer, VkDeviceSize *offset)
{
	if (size == 0)
		return false;

	if (alignment > 1)
		ring.head = (ring.head + alignment - 1) & ~(alignment - 1);

	if (ring.head + size <= ring.buffer.size)
	{
		if (data != nullptr)
			std::memcpy((uint8_t *)ring.buffer.mapped + ring.head, data, (size_t)size);
		*buffer	= ring.buffer.buffer;
		*offset	= ring.head;
		ring.head += size;
		return true;
	}

	/*
	 * Too big for the ring (a full RAM vertex upload is ~19 MB against an 8 MB
	 * ring). Give this one request a buffer of its own; it is freed the next
	 * time this slot comes round, which is after its fence has signalled.
	 */
	FrameSlot &slot = m_slots[m_slot];
	slot.oversized.emplace_back();
	Buffer &dedicated = slot.oversized.back();
	if (!CreateBuffer(dedicated, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
	{
		slot.oversized.pop_back();
		return false;
	}
	if (data != nullptr)
		std::memcpy(dedicated.mapped, data, (size_t)size);
	*buffer	= dedicated.buffer;
	*offset	= 0;
	return true;
}

void VulkanContext::UploadImage(Image &image, uint32_t level, int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t rowLengthTexels, const void *src, size_t srcBytes)
{
	if (!m_frameOpen || !image.Valid() || src == nullptr || srcBytes == 0)
		return;

	VkBuffer		staging	= VK_NULL_HANDLE;
	VkDeviceSize	offset	= 0;
	if (!RingAppend(m_slots[m_slot].staging, src, (VkDeviceSize)srcBytes, 16, &staging, &offset))
	{
		ErrorLog("Vulkan: could not stage %zu bytes for a texture upload.\n", srcBytes);
		return;
	}

	EndPass();							// a copy may not happen inside a render pass
	TransitionImage(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkBufferImageCopy copy = {};
	copy.bufferOffset		= offset;
	copy.bufferRowLength	= rowLengthTexels;	// 0 means "tight", exactly like GL_UNPACK_ROW_LENGTH 0
	copy.bufferImageHeight	= 0;
	copy.imageSubresource.aspectMask	= image.aspect;
	copy.imageSubresource.mipLevel		= level;
	copy.imageSubresource.layerCount	= 1;
	copy.imageOffset		= { x, y, 0 };
	copy.imageExtent		= { width, height, 1 };

	m_vk.vkCmdCopyBufferToImage(Cmd(), staging, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

	TransitionImage(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void VulkanContext::UploadBuffer(Buffer &buffer, VkDeviceSize offset, VkDeviceSize size, const void *src)
{
	if (!m_frameOpen || !buffer.Valid() || src == nullptr || size == 0)
		return;

	VkBuffer		staging			= VK_NULL_HANDLE;
	VkDeviceSize	stagingOffset	= 0;
	if (!RingAppend(m_slots[m_slot].staging, src, size, 16, &staging, &stagingOffset))
	{
		ErrorLog("Vulkan: could not stage %llu bytes for a buffer upload.\n", (unsigned long long)size);
		return;
	}

	EndPass();

	VkBufferCopy copy = {};
	copy.srcOffset	= stagingOffset;
	copy.dstOffset	= offset;
	copy.size		= size;
	m_vk.vkCmdCopyBuffer(Cmd(), staging, buffer.buffer, 1, &copy);
}

void VulkanContext::CopyImage(Image &src, Image &dst, VkImageAspectFlags aspects)
{
	if (!m_frameOpen || !src.Valid() || !dst.Valid())
		return;

	EndPass();

	const VkImageLayout srcWas = src.layout;
	const VkImageLayout dstWas = dst.layout;
	TransitionImage(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	TransitionImage(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkImageCopy copy = {};
	copy.srcSubresource.aspectMask	= aspects;
	copy.srcSubresource.layerCount	= 1;
	copy.dstSubresource.aspectMask	= aspects;
	copy.dstSubresource.layerCount	= 1;
	copy.extent = { src.width < dst.width ? src.width : dst.width,
					src.height < dst.height ? src.height : dst.height, 1 };

	m_vk.vkCmdCopyImage(Cmd(),
		src.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		dst.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &copy);

	// Put both back where the caller had them, so a depth copy in the middle of
	// a frame does not disturb the pass that follows it.
	if (srcWas != VK_IMAGE_LAYOUT_UNDEFINED)
		TransitionImage(src, srcWas);
	if (dstWas != VK_IMAGE_LAYOUT_UNDEFINED)
		TransitionImage(dst, dstWas);
}

void VulkanContext::ReadbackToBuffer(Image &image, VkImageAspectFlags aspect, int32_t x, int32_t y, Buffer &dst, VkDeviceSize dstOffset)
{
	if (!m_frameOpen || !image.Valid() || !dst.Valid())
		return;

	EndPass();

	const VkImageLayout was = image.layout;
	TransitionImage(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	VkBufferImageCopy copy = {};
	copy.bufferOffset	= dstOffset;
	copy.imageSubresource.aspectMask	= aspect;
	copy.imageSubresource.layerCount	= 1;
	copy.imageOffset	= { x, y, 0 };
	copy.imageExtent	= { 1, 1, 1 };

	m_vk.vkCmdCopyImageToBuffer(Cmd(), image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.buffer, 1, &copy);

	if (was != VK_IMAGE_LAYOUT_UNDEFINED)
		TransitionImage(image, was);
}

UniformSlice VulkanContext::AppendUniforms(const void *data, size_t size)
{
	UniformSlice slice;
	if (!m_frameOpen || data == nullptr || size == 0)
		return slice;

	VkDeviceSize alignment = m_deviceProperties.limits.minUniformBufferOffsetAlignment;
	if (alignment < 16)
		alignment = 16;

	VkBuffer		buffer	= VK_NULL_HANDLE;
	VkDeviceSize	offset	= 0;
	if (!RingAppend(m_slots[m_slot].uniforms, data, (VkDeviceSize)size, alignment, &buffer, &offset))
		return slice;

	slice.buffer	= buffer;
	slice.offset	= (uint32_t)offset;
	slice.valid		= true;
	return slice;
}

/******************************************************************************
 Descriptors
******************************************************************************/

VkDescriptorSetLayout VulkanContext::CreateDescriptorSetLayout(const DescriptorBinding *bindings, size_t count)
{
	if (!m_ready || bindings == nullptr || count == 0)
		return VK_NULL_HANDLE;

	std::vector<VkDescriptorSetLayoutBinding> vkBindings(count);
	for (size_t i = 0; i < count; i++)
	{
		vkBindings[i].binding			= bindings[i].binding;
		vkBindings[i].descriptorType	= bindings[i].type;
		vkBindings[i].descriptorCount	= bindings[i].count;
		vkBindings[i].stageFlags		= bindings[i].stages;
	}

	VkDescriptorSetLayoutCreateInfo info = {};
	info.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount	= (uint32_t)count;
	info.pBindings		= vkBindings.data();

	VkDescriptorSetLayout layout = VK_NULL_HANDLE;
	if (m_vk.vkCreateDescriptorSetLayout(m_device, &info, nullptr, &layout) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: could not create a descriptor set layout.\n");
		return VK_NULL_HANDLE;
	}
	return layout;
}

void VulkanContext::DestroyDescriptorSetLayout(VkDescriptorSetLayout layout)
{
	if (m_device != VK_NULL_HANDLE && layout != VK_NULL_HANDLE)
		m_vk.vkDestroyDescriptorSetLayout(m_device, layout, nullptr);
}

VkDescriptorSet VulkanContext::AllocateDescriptorSet(VkDescriptorSetLayout layout)
{
	if (!m_frameOpen || layout == VK_NULL_HANDLE)
		return VK_NULL_HANDLE;

	VkDescriptorSetAllocateInfo info = {};
	info.sType				= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	info.descriptorPool		= m_slots[m_slot].descriptors;
	info.descriptorSetCount	= 1;
	info.pSetLayouts		= &layout;

	VkDescriptorSet set = VK_NULL_HANDLE;
	if (m_vk.vkAllocateDescriptorSets(m_device, &info, &set) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: this frame's descriptor pool is exhausted.\n");
		return VK_NULL_HANDLE;
	}
	return set;
}

void VulkanContext::UpdateDescriptorBuffer(VkDescriptorSet set, uint32_t binding, VkDescriptorType type, VkBuffer buffer, VkDeviceSize range)
{
	if (set == VK_NULL_HANDLE || buffer == VK_NULL_HANDLE)
		return;

	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer	= buffer;
	bufferInfo.offset	= 0;
	bufferInfo.range	= range;

	VkWriteDescriptorSet write = {};
	write.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet			= set;
	write.dstBinding		= binding;
	write.descriptorCount	= 1;
	write.descriptorType	= type;
	write.pBufferInfo		= &bufferInfo;

	m_vk.vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void VulkanContext::UpdateDescriptorImage(VkDescriptorSet set, uint32_t binding, VkImageView view, VkSampler sampler)
{
	UpdateDescriptorImages(set, binding, &view, 1, sampler);
}

void VulkanContext::UpdateDescriptorImages(VkDescriptorSet set, uint32_t binding, const VkImageView *views, uint32_t count, VkSampler sampler)
{
	if (set == VK_NULL_HANDLE || views == nullptr || count == 0)
		return;

	std::vector<VkDescriptorImageInfo> infos(count);
	for (uint32_t i = 0; i < count; i++)
	{
		infos[i].sampler		= sampler;
		infos[i].imageView		= views[i];
		infos[i].imageLayout	= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	VkWriteDescriptorSet write = {};
	write.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet			= set;
	write.dstBinding		= binding;
	write.descriptorCount	= count;
	write.descriptorType	= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo		= infos.data();

	m_vk.vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

} // namespace Vulkan
