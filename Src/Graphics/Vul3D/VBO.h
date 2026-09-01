#ifndef _VBO_H_
#define _VBO_H_

#include "Graphics/Vulkan/VulkanContext.h"

namespace Vul3D {

class VBO
{
public:
	VBO();

	void Create			(VkDeviceSize size, const void* data=nullptr);
	void BufferSubData	(VkDeviceSize offset, VkDeviceSize size, const void* data);
	bool AppendData		(VkDeviceSize size, const void* data);
	void Reset			();		// don't delete data, just go back to start
	void Destroy		();
	void Bind			(bool enable);
	int  GetSize		() const;
	int  GetCapacity	() const;

private:
	Vulkan::Buffer	m_buffer;
	int				m_capacity;
	int				m_size;
};

} // Vul3D

#endif
