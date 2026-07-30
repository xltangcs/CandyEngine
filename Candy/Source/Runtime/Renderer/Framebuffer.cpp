#include "CandyPCH.h"

#include "Runtime/Renderer/Framebuffer.h"
#include "Runtime/Renderer/Renderer.h"

#include "Runtime/Core/Application.h"
#include "Platform/OpenGL/OpenGLFramebuffer.h"
#include <Windows.h>
#include "Platform/D3D12/D3D12Framebuffer.h"
#include "Platform/D3D12/D3D12GraphicsContext.h"
#include "Platform/Vulkan/VulkanFramebuffer.h"
#include "Platform/Vulkan/VulkanGraphicsContext.h"

namespace Candy {

	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:    CANDY_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
		case RendererAPI::API::OpenGL:  return CreateRef<OpenGLFramebuffer>(spec);
		case RendererAPI::API::D3D12:
		{
			// D3D12Framebuffer needs the D3D12Device for resource creation.
			// Try to get it from the active GraphicsContext.
			auto* ctx = Application::Get().GetWindow().GetGraphicsContext();
			auto* d3d12Ctx = dynamic_cast<D3D12GraphicsContext*>(ctx);
			if (d3d12Ctx)
				return CreateRef<D3D12Framebuffer>(spec, d3d12Ctx->GetDevice());
			CANDY_CORE_ERROR("Framebuffer::Create: D3D12 API selected but no D3D12GraphicsContext found");
			return nullptr;
		}
		case RendererAPI::API::Vulkan:
		{
			auto* ctx = Application::Get().GetWindow().GetGraphicsContext();
			auto* vkCtx = dynamic_cast<VulkanGraphicsContext*>(ctx);
			if (vkCtx)
				return CreateRef<VulkanFramebuffer>(spec, vkCtx->GetDevice());
			CANDY_CORE_ERROR("Framebuffer::Create: Vulkan API selected but no VulkanGraphicsContext found");
			return nullptr;
		}
		}

		CANDY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}