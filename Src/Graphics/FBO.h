#ifndef _FBO_H_
#define _FBO_H_

#ifdef SUPERMODEL_VULKAN

// RENDER_BACKEND=vulkan: the Vulkan mirror of this class stands in for it
// everywhere. See Src/Graphics/Vul3D/README.md.
#include "Vulkan/FBO.h"

#else

#include <GL/glew.h>
#include "Supermodel.h"

class FBO
{
public:

	FBO();

	bool	Create(int width, int height);
	void	Destroy();
	void	BindTexture();
	void	Set();
	void	Disable();
	GLuint	GetFBOID();
	GLuint	GetTextureID();

private:

	void	CreateTexture(int width, int height);

	GLuint m_frameBufferID;
	GLuint m_textureID;
};

#endif	// SUPERMODEL_VULKAN

#endif
