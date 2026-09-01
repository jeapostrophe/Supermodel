#include "TextureBank.h"

Vul3D::TextureBank::TextureBank()
{
	// GL_R16UI + GL_NEAREST/GL_NEAREST_MIPMAP_NEAREST + GL_REPEAT: the whole
	// sampler side is moot, because the shaders only ever texelFetch. What does
	// matter is the format and the mip count, and Vulkan allocates the whole
	// chain in one call rather than a glTexImage2D per level -- so the loop
	// below no longer allocates, it only counts, exactly as GL counted.
	int width = 2048;
	int height = 1024;
	int level = 0;

	while (width>=1 && height>=1) {

		width	= (width > 1) ? width / 2 : 1;
		height	= (height > 1) ? height / 2 : 1;

		level++;

		if (width == 1 && height == 1) {
			break;
		}
	}

	m_numLevels = level;

	// The chain has one more level than GL reports: the GL loop allocates a
	// final 1x1 level AFTER the increment that produced m_numLevels, and
	// CNew3D::UploadTextures iterates GetNumberOfLevels(), so the extra level
	// exists but is never uploaded. Vulkan needs the real count.
	Vulkan::VulkanContext::Get().CreateImage(m_texture, 2048, 1024, m_numLevels + 1,
		VK_FORMAT_R16_UINT,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT);
}

Vul3D::TextureBank::~TextureBank()
{
	Vulkan::VulkanContext::Get().DestroyImage(m_texture);
}

void Vul3D::TextureBank::AttachMemory(const UINT16* textureRam)
{
	m_textureRam = textureRam;
}

VkImageView Vul3D::TextureBank::Bind()
{
	// There is no bind point: R3DShader points a descriptor at this view.
	return m_texture.view;
}

void Vul3D::TextureBank::UploadTextures(int level, int x, int y, int width, int height)
{
	if (m_textureRam == nullptr || width <= 0 || height <= 0) {
		return;
	}

	static constexpr int mipXBase[] = { 0, 1024, 1536, 1792, 1920, 1984, 2016, 2032, 2040, 2044, 2046, 2047 };
	static constexpr int mipYBase[] = { 0, 512, 768, 896, 960, 992, 1008, 1016, 1020, 1022, 1023 };

	int subX = x - mipXBase[level];
	int subY = y - mipYBase[level];

	// GL walked one row at a time because glPixelStorei(GL_UNPACK_ROW_LENGTH)
	// was not set; Vulkan carries the source stride in the copy itself
	// (bufferRowLength), so the whole sub-rectangle goes in one copy. Texture
	// RAM is 2048 texels wide, which is that stride.
	const size_t srcBytes = ((size_t)(height - 1) * 2048 + (size_t)width) * sizeof(UINT16);

	Vulkan::VulkanContext::Get().UploadImage(m_texture, (uint32_t)level, subX, subY,
		(uint32_t)width, (uint32_t)height, 2048,
		m_textureRam + ((size_t)y * 2048) + x, srcBytes);
}

int Vul3D::TextureBank::GetNumberOfLevels() const
{
	return m_numLevels;
}
