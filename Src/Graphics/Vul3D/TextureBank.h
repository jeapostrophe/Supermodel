#pragma once

#ifndef INCLUDED_VUL3D_TEXTUREBANK_H
#define INCLUDED_VUL3D_TEXTUREBANK_H

#include "Types.h"
#include "Graphics/Vulkan/VulkanContext.h"

// texture banks are a fixed size
// 2048x1024 pixels, each pixel is 16bits in size

namespace Vul3D {

	class TextureBank
	{
	public:
		TextureBank();
		~TextureBank();

		void AttachMemory(const UINT16* textureRam);
		VkImageView Bind();
		void UploadTextures(int level, int x, int y, int width, int height);
		int GetNumberOfLevels() const;

	private:
		const UINT16* m_textureRam = nullptr;
		Vulkan::Image m_texture;
		int m_numLevels = 0;
	};

}

#endif
