#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/RHI/RHIFramebuffer.h"

namespace Candy {

	// =========================================================================
	// Framebuffer — engine-level off-screen render target
	//
	// Single-inherits RHIFramebuffer: runtime/editor code holding a
	// Ref<Framebuffer> can pass it anywhere a Ref<RHIFramebuffer> is expected
	// (Renderer2D::SetActiveRenderTarget, RHICommandBuffer::BeginRenderPass)
	// without dynamic_pointer_cast bridges.
	//
	// FramebufferDesc (RHI side) is the single source of truth for the
	// specification — there is no separate engine-level spec struct.
	// =========================================================================
	class Framebuffer : public RHIFramebuffer
	{
	public:
		virtual ~Framebuffer() = default;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		// RHIFramebuffer interface (GetDesc / Resize / GetWidth / GetHeight /
		// GetColorAttachmentCount / HasDepthStencil) is implemented by backends.

		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) = 0;
		virtual void ClearAttachment(uint32_t attachmentIndex, int value) = 0;
		virtual uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const = 0;

		/// Returns a 64-bit GPU handle for the color attachment SRV.
		/// In D3D12, this is the D3D12_GPU_DESCRIPTOR_HANDLE.ptr (used as ImTextureID).
		/// In OpenGL, returns GetColorAttachmentRendererID() zero-extended.
		virtual uint64_t GetColorAttachmentGPUHandle(uint32_t index = 0) const = 0;

		static Ref<Framebuffer> Create(const FramebufferDesc& desc);
	};
}
