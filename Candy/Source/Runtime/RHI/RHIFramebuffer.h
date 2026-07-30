#pragma once

#include "Runtime/RHI/RHITypes.h"

#include <vector>
#include <cstdint>

namespace Candy {

	// =========================================================================
	// FramebufferAttachmentDesc — describes a single color/depth attachment
	// =========================================================================
	struct FramebufferAttachmentDesc
	{
		RHIFormat Format    = RHIFormat::R8G8B8A8Unorm;
		bool      IsInteger = false;  ///< e.g. RED_INTEGER for entity-picking targets
	};

	// =========================================================================
	// FramebufferDesc — immutable description used to create an RHIFramebuffer
	// =========================================================================
	struct FramebufferDesc
	{
		uint32_t                              Width             = 1;
		uint32_t                              Height            = 1;
		std::vector<FramebufferAttachmentDesc> ColorAttachments;
		FramebufferAttachmentDesc             DepthStencilAttachment;
		bool                                  HasDepthStencil   = false;
		uint32_t                              SampleCount       = 1;
		/// When true, the framebuffer aliases the swap chain's back buffer
		/// (no private color resources are allocated).
		bool                                  SwapChainTarget   = false;
	};

	// =========================================================================
	// RHIFramebuffer — opaque off-screen render target
	//
	// Backends own the concrete RTV/DSV (D3D12) / FBO (OpenGL) / VkFramebuffer
	// (Vulkan). Runtime code only sees this abstract interface plus the
	// descriptor returned by RHIDevice::CreateFramebuffer.
	// =========================================================================
	class RHIFramebuffer
	{
	public:
		virtual ~RHIFramebuffer() = default;

		[[nodiscard]] virtual const FramebufferDesc& GetDesc() const = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;

		[[nodiscard]] virtual uint32_t GetWidth()  const = 0;
		[[nodiscard]] virtual uint32_t GetHeight() const = 0;

		[[nodiscard]] virtual uint32_t GetColorAttachmentCount() const = 0;
		[[nodiscard]] virtual bool     HasDepthStencil()         const = 0;
	};

} // namespace Candy
