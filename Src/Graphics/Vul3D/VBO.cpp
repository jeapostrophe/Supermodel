#include "VBO.h"

namespace Vul3D {

VBO::VBO()
{
	m_capacity	= 0;
	m_size		= 0;
}

void VBO::Create(VkDeviceSize size, const void* data)
{
	// GL's target and usage have no Vulkan counterpart: what the buffer may be
	// used for is fixed at creation, and DEVICE_LOCAL + TRANSFER_DST is what
	// GL_ARRAY_BUFFER / GL_DYNAMIC_DRAW plus glBufferSubData asks for.
	Vulkan::VulkanContext::Get().CreateBuffer(m_buffer, size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	m_capacity	= (int)size;
	m_size		= 0;

	if (data) {
		BufferSubData(0, size, data);
	}

	Bind(false);		// unbind
}

void VBO::BufferSubData(VkDeviceSize offset, VkDeviceSize size, const void* data)
{
	Vulkan::VulkanContext::Get().UploadBuffer(m_buffer, offset, size, data);
}

bool VBO::AppendData(VkDeviceSize size, const void* data)
{
	if (size == 0 || !data) {
		return true;	// nothing to do
	}

	if (m_size + (int)size >= m_capacity) {
		return false;
	}

	BufferSubData(m_size, size, data);

	m_size += (int)size;

	return true;
}

void VBO::Reset()
{
	m_size = 0;
}

void VBO::Destroy()
{
	Vulkan::VulkanContext::Get().DestroyBuffer(m_buffer);
	m_capacity	= 0;
	m_size		= 0;
}

void VBO::Bind(bool enable)
{
	if (enable) {
		Vulkan::VulkanContext::Get().BindVertexBuffer(m_buffer.buffer, 0);
	}
	else {
		// Vulkan has no "bind zero": leaving the binding in place is harmless,
		// because a draw only reads it through a pipeline that declares it.
	}
}

int VBO::GetSize() const
{
	return m_size;
}

int VBO::GetCapacity() const
{
	return m_capacity;
}

} // Vul3D
