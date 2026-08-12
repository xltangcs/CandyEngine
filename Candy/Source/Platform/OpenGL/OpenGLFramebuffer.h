#pragma once

#include "Runtime/Renderer/Framebuffer.h"

namespace Candy {

	class OpenGLFramebuffer : public Framebuffer
	{
	public:
		OpenGLFramebuffer(const FramebufferDesc& desc);
		virtual ~OpenGLFramebuffer();

		void Invalidate();

		virtual void Bind() override;
		virtual void Unbind() override;
		virtual void Resize(uint32_t width, uint32_t height) override;
		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) override;
		virtual void ClearAttachment(uint32_t attachmentIndex, int value) override;
		virtual uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const override
		{
			if (m_Desc.SwapChainTarget) return 0;
			CANDY_CORE_ASSERT(index < m_ColorAttachments.size());
			return m_ColorAttachments[index];
		}

		virtual uint64_t GetColorAttachmentGPUHandle(uint32_t index = 0) const override
		{
			return static_cast<uint64_t>(GetColorAttachmentRendererID(index));
		}

		bool IsSwapChainTarget() const { return m_Desc.SwapChainTarget; }

		/// Expose the internal GL FBO id so the OpenGL RHI command buffer can
		/// route RenderPass setup through RHI-side SetFramebufferRenderTarget.
		uint32_t GetNativeFBO() const { return m_RendererID; }

		// ---- RHIFramebuffer (via Framebuffer) ---------------------------
		const FramebufferDesc& GetDesc() const override { return m_Desc; }
		uint32_t GetWidth()  const override { return m_Desc.Width;  }
		uint32_t GetHeight() const override { return m_Desc.Height; }
		uint32_t GetColorAttachmentCount() const override { return static_cast<uint32_t>(m_ColorAttachments.size()); }
		bool     HasDepthStencil() const override { return m_Desc.HasDepthStencil; }

	private:
		uint32_t m_RendererID = 0;
		uint32_t m_ColorAttachment = 0;
		uint32_t m_DepthAttachment = 0;
		FramebufferDesc m_Desc;

		std::vector<uint32_t> m_ColorAttachments;
	};

}
