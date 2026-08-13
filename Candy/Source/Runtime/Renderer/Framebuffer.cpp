#include "CandyPCH.h"

#include "Runtime/Renderer/Framebuffer.h"

#include "Runtime/RHI/RHIContext.h"
#include "Runtime/RHI/RHIDevice.h"

namespace Candy {

	Ref<Framebuffer> Framebuffer::Create(const FramebufferDesc& desc)
	{
		// The RHI device is the resource factory — no backend headers or
		// GraphicsContext downcasts needed here. The returned object is the
		// backend's Framebuffer subclass (is-a RHIFramebuffer via the
		// Framebuffer base), so the downcast is only a type-level adjustment.
		auto* dev = RHIContext::GetDevice();
		if (!dev)
		{
			CANDY_CORE_ERROR("Framebuffer::Create: no RHI device published");
			return nullptr;
		}

		auto fb = dev->CreateFramebuffer(desc);
		return std::dynamic_pointer_cast<Framebuffer>(fb);
	}

}
