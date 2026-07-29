#include "CandyPCH.h"

#include "Runtime/Renderer/Framebuffer.h"
#include "Runtime/Renderer/Renderer.h"

#include "Runtime/Core/Application.h"
#include "Platform/OpenGL/OpenGLFramebuffer.h"
#include <Windows.h>
#include "Platform/DX12/DX12Framebuffer.h"
#include "Platform/DX12/DX12GraphicsContext.h"

namespace Candy {

	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:    CANDY_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
		case RendererAPI::API::OpenGL:  return CreateRef<OpenGLFramebuffer>(spec);
		case RendererAPI::API::DX12:
		{
			// DX12Framebuffer needs the DX12Device for resource creation.
			// Try to get it from the active GraphicsContext.
			auto* ctx = Application::Get().GetWindow().GetGraphicsContext();
			auto* dx12Ctx = dynamic_cast<DX12GraphicsContext*>(ctx);
			if (dx12Ctx)
				return CreateRef<DX12Framebuffer>(spec, dx12Ctx->GetDevice());
			CANDY_CORE_ERROR("Framebuffer::Create: DX12 API selected but no DX12GraphicsContext found");
			return nullptr;
		}
		}

		CANDY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}