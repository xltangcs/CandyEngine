#include "CandyPCH.h"

#include "Platform/OpenGL/OpenGLRHIResources.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	// =========================================================================
	// OpenGLRHIBuffer
	// =========================================================================
	OpenGLRHIBuffer::OpenGLRHIBuffer(const BufferDesc& desc)
		: m_Desc(desc)
	{
		GLenum target = GetTarget();
		glCreateBuffers(1, &m_ID);
		glNamedBufferData(m_ID, static_cast<GLsizei>(m_Desc.Size), nullptr, GL_DYNAMIC_DRAW);
		CANDY_CORE_TRACE("OpenGLRHIBuffer: created '{}' ({} bytes, target=0x{:x})",
		                  m_Desc.DebugName, m_Desc.Size, target);
	}

	OpenGLRHIBuffer::~OpenGLRHIBuffer()
	{
		if (m_ID)
		{
			if (m_Mapped)
				Unmap();
			glDeleteBuffers(1, &m_ID);
		}
	}

	GLenum OpenGLRHIBuffer::GetTarget() const
	{
		if (HasFlag(m_Desc.Usage, ResourceUsage::VertexBuffer))
			return GL_ARRAY_BUFFER;
		if (HasFlag(m_Desc.Usage, ResourceUsage::IndexBuffer))
			return GL_ELEMENT_ARRAY_BUFFER;
		if (HasFlag(m_Desc.Usage, ResourceUsage::ConstantBuffer))
			return GL_UNIFORM_BUFFER;
		return GL_ARRAY_BUFFER;
	}

	void* OpenGLRHIBuffer::Map()
	{
		if (!m_Mapped && m_ID)
		{
			m_Mapped = glMapNamedBufferRange(m_ID, 0, static_cast<GLsizeiptr>(m_Desc.Size), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		}
		return m_Mapped;
	}

	void OpenGLRHIBuffer::Unmap()
	{
		if (m_Mapped && m_ID)
		{
			glUnmapNamedBuffer(m_ID);
			m_Mapped = nullptr;
		}
	}

	// =========================================================================
	// OpenGLRHISampler
	// =========================================================================
	OpenGLRHISampler::OpenGLRHISampler(const SamplerDesc& desc)
		: m_Desc(desc)
	{
		glCreateSamplers(1, &m_ID);
		glSamplerParameteri(m_ID, GL_TEXTURE_MIN_FILTER, ToGLFilter(desc.MinFilter));
		glSamplerParameteri(m_ID, GL_TEXTURE_MAG_FILTER, ToGLFilter(desc.MagFilter));
		glSamplerParameteri(m_ID, GL_TEXTURE_WRAP_S, ToGLAddress(desc.AddressU));
		glSamplerParameteri(m_ID, GL_TEXTURE_WRAP_T, ToGLAddress(desc.AddressV));
		glSamplerParameteri(m_ID, GL_TEXTURE_WRAP_R, ToGLAddress(desc.AddressW));
	}

	OpenGLRHISampler::~OpenGLRHISampler()
	{
		if (m_ID)
			glDeleteSamplers(1, &m_ID);
	}

	// =========================================================================
	// OpenGLRHITexture2D
	// =========================================================================
	OpenGLRHITexture2D::OpenGLRHITexture2D(const TextureDesc& desc)
		: m_Desc(desc)
	{
		m_InternalFormat = ToGLInternalFormat(desc.Format);
		m_Format         = ToGLPixelFormat(desc.Format);
		m_Type           = ToGLPixelType(desc.Format);

		glCreateTextures(GL_TEXTURE_2D, 1, &m_ID);

		GLsizei levels = static_cast<GLsizei>(desc.MipLevels > 0 ? desc.MipLevels : 1);
		glTextureStorage2D(m_ID, levels, m_InternalFormat,
		                   static_cast<GLsizei>(desc.Width),
		                   static_cast<GLsizei>(desc.Height));

		glTextureParameteri(m_ID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_ID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(m_ID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_ID, GL_TEXTURE_WRAP_T, GL_REPEAT);

		CANDY_CORE_TRACE("OpenGLRHITexture2D: created {}x{} (fmt={})",
		                 desc.Width, desc.Height, static_cast<int>(desc.Format));
	}

	OpenGLRHITexture2D::~OpenGLRHITexture2D()
	{
		if (m_ID)
			glDeleteTextures(1, &m_ID);
	}

	void OpenGLRHITexture2D::SetData(const void* data, uint32_t size)
	{
		(void)size;
		if (!m_ID || !data)
			return;
		GLsizei w = static_cast<GLsizei>(m_Desc.Width);
		GLsizei h = static_cast<GLsizei>(m_Desc.Height);
		GLint level = 0;
		// 1 mip: use glTextureSubImage2D straight away
		glTextureSubImage2D(m_ID, level, 0, 0, w, h, m_Format, m_Type, data);
	}

	// =========================================================================
	// OpenGLRHIFramebuffer
	// =========================================================================
	OpenGLRHIFramebuffer::OpenGLRHIFramebuffer(const FramebufferDesc& desc)
		: m_Desc(desc)
	{
		Invalidate();
	}

	OpenGLRHIFramebuffer::~OpenGLRHIFramebuffer()
	{
		if (m_FBO)
			glDeleteFramebuffers(1, &m_FBO);
		if (!m_ColorTextures.empty())
			glDeleteTextures(static_cast<GLsizei>(m_ColorTextures.size()), m_ColorTextures.data());
		if (m_DepthTexture)
			glDeleteTextures(1, &m_DepthTexture);
	}

	void OpenGLRHIFramebuffer::Invalidate()
	{
		if (m_FBO)
			glDeleteFramebuffers(1, &m_FBO);
		for (GLuint id : m_ColorTextures)
			if (id) glDeleteTextures(1, &id);
		m_ColorTextures.clear();
		if (m_DepthTexture)
		{
			glDeleteTextures(1, &m_DepthTexture);
			m_DepthTexture = 0;
		}

		glCreateFramebuffers(1, &m_FBO);

		m_ColorTextures.resize(m_Desc.ColorAttachments.size());
		for (uint32_t i = 0; i < m_Desc.ColorAttachments.size(); ++i)
		{
			GLenum internalFmt = ToGLInternalFormat(m_Desc.ColorAttachments[i].Format);
			glCreateTextures(GL_TEXTURE_2D, 1, &m_ColorTextures[i]);
			glTextureStorage2D(m_ColorTextures[i], 1, internalFmt,
			                   static_cast<GLsizei>(m_Desc.Width),
			                   static_cast<GLsizei>(m_Desc.Height));
			glNamedFramebufferTexture(m_FBO, GL_COLOR_ATTACHMENT0 + i, m_ColorTextures[i], 0);
		}

		if (m_Desc.HasDepthStencil)
		{
			GLenum internalFmt = ToGLInternalFormat(m_Desc.DepthStencilAttachment.Format);
			glCreateTextures(GL_TEXTURE_2D, 1, &m_DepthTexture);
			glTextureStorage2D(m_DepthTexture, 1, internalFmt,
			                   static_cast<GLsizei>(m_Desc.Width),
			                   static_cast<GLsizei>(m_Desc.Height));
			glNamedFramebufferTexture(m_FBO, GL_DEPTH_ATTACHMENT, m_DepthTexture, 0);
		}

		// 验证完整性
		if (m_ColorTextures.empty())
			glNamedFramebufferDrawBuffer(m_FBO, GL_NONE);
		else
		{
			GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
			uint32_t count = (uint32_t)m_ColorTextures.size();
			if (count > 4) count = 4;
			glNamedFramebufferDrawBuffers(m_FBO, count, buffers);
		}

		GLenum status = glCheckNamedFramebufferStatus(m_FBO, GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
			CANDY_CORE_ERROR("OpenGLRHIFramebuffer: incomplete (0x{:x})", status);
	}

	void OpenGLRHIFramebuffer::Resize(uint32_t width, uint32_t height)
	{
		if (m_Desc.Width == width && m_Desc.Height == height)
			return;
		m_Desc.Width  = width;
		m_Desc.Height = height;
		Invalidate();
	}

	GLuint OpenGLRHIFramebuffer::GetColorTextureID(uint32_t index) const
	{
		if (index >= m_ColorTextures.size())
			return 0;
		return m_ColorTextures[index];
	}

	// =========================================================================
	// OpenGLRHISwapChain
	// =========================================================================
	OpenGLRHISwapChain::OpenGLRHISwapChain(const SwapChainDesc& desc)
		: m_Desc(desc)
	{
	}

	void OpenGLRHISwapChain::Resize(uint32_t width, uint32_t height)
	{
		m_Desc.Width  = width;
		m_Desc.Height = height;
	}

	// =========================================================================
	// OpenGLRHIGraphicsPipeline
	// =========================================================================
	OpenGLRHIGraphicsPipeline::OpenGLRHIGraphicsPipeline(const GraphicsPipelineDesc& desc)
		: m_Desc(desc)
	{
	}

	OpenGLRHIGraphicsPipeline::~OpenGLRHIGraphicsPipeline()
	{
		if (m_Program)
			glDeleteProgram(m_Program);
	}

	void OpenGLRHIGraphicsPipeline::ApplyState() const
	{
		// Rasterizer
		switch (m_Desc.Rasterizer.Cull)
		{
		case CullMode::None:  glDisable(GL_CULL_FACE); break;
		case CullMode::Front: glEnable(GL_CULL_FACE); glCullFace(GL_FRONT); break;
		case CullMode::Back:  glEnable(GL_CULL_FACE); glCullFace(GL_BACK); break;
		}
		glPolygonMode(GL_FRONT_AND_BACK, m_Desc.Rasterizer.Fill == FillMode::Wireframe ? GL_LINE : GL_FILL);

		// Depth
		if (m_Desc.DepthStencil.DepthTestEnable)
		{
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_NEVER + static_cast<GLenum>(m_Desc.DepthStencil.DepthCompareOp));
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}
		glDepthMask(m_Desc.DepthStencil.DepthWriteEnable ? GL_TRUE : GL_FALSE);

		// Blend
		if (m_Desc.Blend.BlendEnable)
		{
			glEnable(GL_BLEND);
			// Simplified alpha blend (SrcAlpha, OneMinusSrcAlpha) to match the
			// common 2D quad case.  Full BlendFactor mapping deferred.
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
		}
		else
		{
			glDisable(GL_BLEND);
		}
	}

} // namespace Candy
