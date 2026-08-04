#pragma once

#include "Runtime/RHI/RHIDevice.h"
#include "Runtime/RHI/RHIFramebuffer.h"
#include "Runtime/RHI/RHISwapChain.h"
#include "Runtime/RHI/RHIPipelineState.h"
#include "Runtime/RHI/RHIShader.h"
#include "Runtime/RHI/RHITypes.h"
#include "Runtime/RHI/RHICommandQueue.h"

#include <glad/glad.h>
#include <string>
#include <vector>

namespace Candy {

	// =========================================================================
	// Format / state helpers shared across OpenGL RHI types
	// =========================================================================
	inline GLenum ToGLInternalFormat(RHIFormat fmt)
	{
		switch (fmt)
		{
		case RHIFormat::R8Unorm:           return GL_R8;
		case RHIFormat::R8Uint:            return GL_R8UI;
		case RHIFormat::R8G8Unorm:          return GL_RG8;
		case RHIFormat::R8G8Uint:          return GL_RG8UI;
		case RHIFormat::R8G8B8A8Unorm:     return GL_RGBA8;
		case RHIFormat::R8G8B8A8Srgb:       return GL_SRGB8_ALPHA8;
		case RHIFormat::B8G8R8A8Unorm:     return GL_RGBA8;
		case RHIFormat::B8G8R8A8Srgb:       return GL_SRGB8_ALPHA8;
		case RHIFormat::R32Float:           return GL_R32F;
		case RHIFormat::R32G32Float:        return GL_RG32F;
		case RHIFormat::R32G32B32A32Float:  return GL_RGBA32F;
		case RHIFormat::R32Sint:           return GL_R32I;
		case RHIFormat::R16G16B16A16Float:  return GL_RGBA16F;
		case RHIFormat::D16Unorm:          return GL_DEPTH_COMPONENT16;
		case RHIFormat::D32Float:          return GL_DEPTH_COMPONENT32F;
		case RHIFormat::D24UnormS8Uint:     return GL_DEPTH24_STENCIL8;
		case RHIFormat::D32FloatS8Uint:     return GL_DEPTH32F_STENCIL8;
		default:                            return GL_RGBA8;
		}
	}

	inline GLenum ToGLPixelFormat(RHIFormat fmt)
	{
		switch (fmt)
		{
		case RHIFormat::R8G8B8A8Unorm:
		case RHIFormat::R8G8B8A8Srgb:
		case RHIFormat::B8G8R8A8Unorm:
		case RHIFormat::B8G8R8A8Srgb:       return GL_RGBA;
		case RHIFormat::R8Unorm:            return GL_RED;
		case RHIFormat::R8G8Unorm:          return GL_RG;
		case RHIFormat::R32Float:           return GL_RED;
		case RHIFormat::R32G32Float:        return GL_RG;
		case RHIFormat::R32G32B32A32Float:
		case RHIFormat::R16G16B16A16Float:  return GL_RGBA;
		case RHIFormat::R32Sint:           return GL_RED_INTEGER;
		case RHIFormat::D16Unorm:
		case RHIFormat::D32Float:
		case RHIFormat::D24UnormS8Uint:
		case RHIFormat::D32FloatS8Uint:     return GL_DEPTH_COMPONENT;
		default:                            return GL_RGBA;
		}
	}

	inline GLenum ToGLPixelType(RHIFormat fmt)
	{
		switch (fmt)
		{
		case RHIFormat::R32Sint:           return GL_INT;
		case RHIFormat::R32Uint:           return GL_UNSIGNED_INT;
		case RHIFormat::R32Float:           return GL_FLOAT;
		case RHIFormat::R8G8B8A8Unorm:
		case RHIFormat::R8G8B8A8Srgb:
		case RHIFormat::B8G8R8A8Unorm:
		case RHIFormat::B8G8R8A8Srgb:       return GL_UNSIGNED_BYTE;
		default:                            return GL_UNSIGNED_BYTE;
		}
	}

	inline GLenum ToGLFilter(SamplerFilter f)        { return f == SamplerFilter::Nearest ? GL_NEAREST : GL_LINEAR; }
	inline GLenum ToGLAddress(SamplerAddressMode m)
	{
		switch (m)
		{
		case SamplerAddressMode::Repeat:         return GL_REPEAT;
		case SamplerAddressMode::MirroredRepeat: return GL_MIRRORED_REPEAT;
		case SamplerAddressMode::ClampToEdge:    return GL_CLAMP_TO_EDGE;
		case SamplerAddressMode::ClampToBorder:  return GL_CLAMP_TO_BORDER;
		default:                                 return GL_REPEAT;
		}
	}

	/// Returns size in bytes for one vertex attribute element; outType/size/normalized
	/// describe the GL vertex attribute pointer setup matching `fmt`.
	inline GLuint ToGLVertexAttrib(RHIFormat fmt, GLenum& outType, GLint& outSize, GLboolean& outNormalized)
	{
		outNormalized = GL_FALSE;
		switch (fmt)
		{
		case RHIFormat::R32Float:           outType = GL_FLOAT; outSize = 1; return 4;
		case RHIFormat::R32G32Float:        outType = GL_FLOAT; outSize = 2; return 4 * 2;
		case RHIFormat::R32G32B32Float:     outType = GL_FLOAT; outSize = 3; return 4 * 3;
		case RHIFormat::R32G32B32A32Float:  outType = GL_FLOAT; outSize = 4; return 4 * 4;
		case RHIFormat::R32Sint:            outType = GL_INT;   outSize = 1; return 4;
		case RHIFormat::R32G32Sint:         outType = GL_INT;   outSize = 2; return 4 * 2;
		case RHIFormat::R32G32B32Sint:       outType = GL_INT;   outSize = 3; return 4 * 3;
		case RHIFormat::R32G32B32A32Sint:   outType = GL_INT;   outSize = 4; return 4 * 4;
		default:                            outType = GL_FLOAT; outSize = 4; return 4 * 4;
		}
	}


	// =========================================================================
	// OpenGLRHIShaderModule — RHIShaderModule subclass that stores the GLSL
	// source string (D3D12 uses DXBC bytecode; reusing the same RHI interface
	// for GLSL source by exposing the source bytes via GetBytecode).
	// =========================================================================
	class OpenGLRHIShaderModule : public RHIShaderModule
	{
	public:
		OpenGLRHIShaderModule(ShaderStage stage, std::string source, std::string name)
			: m_Stage(stage), m_Source(std::move(source)), m_Name(std::move(name)) {}

		ShaderStage              GetStage()        const override { return m_Stage; }
		const uint32_t*          GetBytecode()      const override { return reinterpret_cast<const uint32_t*>(m_Source.data()); }
		uint32_t                 GetBytecodeSize()  const override { return static_cast<uint32_t>(m_Source.size()); }
		const std::string&       GetDebugName()     const override { return m_Name; }

		const std::string&       GetSource() const { return m_Source; }

	private:
		ShaderStage m_Stage  = ShaderStage::None;
		std::string m_Source;
		std::string m_Name;
	};

	// =========================================================================
	// OpenGLRHIBuffer — RHIBuffer backed by a GL buffer object
	// =========================================================================
	class OpenGLRHIBuffer : public RHIBuffer
	{
	public:
		explicit OpenGLRHIBuffer(const BufferDesc& desc);
		~OpenGLRHIBuffer() override;

		const BufferDesc& GetDesc() const override { return m_Desc; }
		void* Map()   override;
		void  Unmap() override;

		GLuint     GetID()        const { return m_ID; }
		/// Stride per vertex when used as a vertex buffer.
		uint32_t   GetStride()   const { return m_Desc.Stride; }
		GLenum     GetTarget()   const;

	private:
		BufferDesc m_Desc;
		GLuint     m_ID = 0;
		void*      m_Mapped = nullptr;
	};

	// =========================================================================
	// OpenGLRHISampler — GL sampler object
	// =========================================================================
	class OpenGLRHISampler : public RHISampler
	{
	public:
		explicit OpenGLRHISampler(const SamplerDesc& desc);
		~OpenGLRHISampler() override;

		const SamplerDesc& GetDesc() const override { return m_Desc; }
		GLuint             GetID()  const { return m_ID; }

	private:
		SamplerDesc m_Desc;
		GLuint      m_ID = 0;
	};

	// =========================================================================
	// OpenGLRHITexture2D — RHITexture backed by a GL 2D texture
	// =========================================================================
	class OpenGLRHITexture2D : public RHITexture
	{
	public:
		explicit OpenGLRHITexture2D(const TextureDesc& desc);
		~OpenGLRHITexture2D() override;

		const TextureDesc& GetDesc() const override { return m_Desc; }

		GLuint GetID() const { return m_ID; }

		/// Upload pixel data (assumes unpack-aligned row data).
		void SetData(const void* data, uint32_t size);

	private:
		TextureDesc m_Desc;
		GLuint      m_ID = 0;
		GLenum      m_InternalFormat = 0;
		GLenum      m_Format         = 0;
		GLenum      m_Type           = 0;
	};

	// =========================================================================
	// OpenGLRHIFramebuffer — RHIFramebuffer backed by a GL FBO
	// =========================================================================
	class OpenGLRHIFramebuffer : public RHIFramebuffer
	{
	public:
		explicit OpenGLRHIFramebuffer(const FramebufferDesc& desc);
		~OpenGLRHIFramebuffer() override;

		const FramebufferDesc& GetDesc() const override { return m_Desc; }
		void Resize(uint32_t width, uint32_t height) override;

		uint32_t GetWidth()  const override { return m_Desc.Width; }
		uint32_t GetHeight() const override { return m_Desc.Height; }
		uint32_t GetColorAttachmentCount() const override { return static_cast<uint32_t>(m_ColorTextures.size()); }
		bool     HasDepthStencil()        const override { return m_Desc.HasDepthStencil; }

		GLuint GetFBO() const { return m_FBO; }
		GLuint GetColorTextureID(uint32_t index = 0) const;
		GLuint GetDepthTextureID() const { return m_DepthTexture; }

	private:
		void Invalidate();

		FramebufferDesc       m_Desc;
		GLuint                 m_FBO = 0;
		std::vector<GLuint>    m_ColorTextures;
		GLuint                 m_DepthTexture = 0;
	};

	// =========================================================================
	// OpenGLRHISwapChain — minimal GL swap chain (no real back-buffer objects;
	// rendering to the default framebuffer is signalled by a null current RT)
	// =========================================================================
	class OpenGLRHISwapChain : public RHISwapChain
	{
	public:
		explicit OpenGLRHISwapChain(const SwapChainDesc& desc);
		~OpenGLRHISwapChain() override = default;

		const SwapChainDesc&  GetDesc() const override { return m_Desc; }
		Ref<RHITexture> GetCurrentBackBuffer() override { return nullptr; }
		void Resize(uint32_t width, uint32_t height) override;
		uint32_t GetWidth()  const override { return m_Desc.Width; }
		uint32_t GetHeight() const override { return m_Desc.Height; }

	private:
		SwapChainDesc m_Desc;
	};

	// =========================================================================
	// OpenGLRHIGraphicsPipeline — RHIGraphicsPipeline wrapping a linked GL
	// program plus the immutable rasterizer/blend/depth state baked from
	// GraphicsPipelineDesc.
	// =========================================================================
	class OpenGLRHIGraphicsPipeline : public RHIGraphicsPipeline
	{
	public:
		explicit OpenGLRHIGraphicsPipeline(const GraphicsPipelineDesc& desc);
		~OpenGLRHIGraphicsPipeline() override;

		const GraphicsPipelineDesc& GetDesc() const { return m_Desc; }
		GLuint                      GetProgram() const { return m_Program; }
		void                        SetProgram(GLuint program) { m_Program = program; }

		/// Re-apply blend / rasterizer / depth-stencil state in Bind().
		void ApplyState() const;

	private:
		GraphicsPipelineDesc m_Desc;
		GLuint               m_Program = 0;
	};

} // namespace Candy
