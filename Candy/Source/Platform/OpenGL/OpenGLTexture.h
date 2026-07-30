#pragma once

#include "Runtime/Renderer/Texture.h"
#include "Runtime/RHI/RHIDevice.h"
#include <glad/glad.h>

namespace Candy {
	class OpenGLTexture2D : public Texture2D, public RHITexture
	{
	public:
		OpenGLTexture2D(uint32_t width, uint32_t height);
		OpenGLTexture2D(const std::string& path);
		virtual ~OpenGLTexture2D();

		virtual uint32_t GetWidth() const override { return m_Width; }
		virtual uint32_t GetHeight() const override { return m_Height; }
		virtual uint32_t GetRendererID() const override { return m_RendererID; }
		virtual uint64_t GetRendererID64() const override { return static_cast<uint64_t>(m_RendererID); }

		// RHITexture override
		const TextureDesc& GetDesc() const override { return m_RHIDesc; }

		virtual void SetData(void* data, uint32_t size) override;

		virtual void Bind(uint32_t slot = 0) const override;
		virtual bool IsLoaded() const override { return m_IsLoaded; }
		virtual bool operator==(const Texture& other) const override
		{
			return m_RendererID == ((OpenGLTexture2D&)other).m_RendererID;
		}
	private:
		std::string m_Path;
		bool m_IsLoaded = false;
		uint32_t m_Width, m_Height;
		uint32_t m_RendererID;
		GLenum m_InternalFormat, m_DataFormat;

		TextureDesc m_RHIDesc;
	};
}
