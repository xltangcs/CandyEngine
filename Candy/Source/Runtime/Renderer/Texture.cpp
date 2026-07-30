#include "CandyPCH.h"

#include "Runtime/Renderer/Texture.h"
#include "Runtime/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLTexture.h"
#include "Platform/D3D12/D3D12Texture2D.h"
#include "Platform/D3D12/D3D12GraphicsContext.h"
#include "Platform/Vulkan/VulkanTexture2D.h"
#include "Platform/Vulkan/VulkanGraphicsContext.h"
#include "Runtime/Core/Application.h"

namespace Candy {

	static D3D12Device* GetD3D12Device()
	{
		auto* ctx = Application::Get().GetWindow().GetGraphicsContext();
		auto* d3d12Ctx = dynamic_cast<D3D12GraphicsContext*>(ctx);
		return d3d12Ctx ? d3d12Ctx->GetDevice() : nullptr;
	}

	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:    CANDY_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
		case RendererAPI::API::OpenGL:  return CreateRef<OpenGLTexture2D>(width, height);
		case RendererAPI::API::D3D12:
		{
			auto* dev = GetD3D12Device();
			if (dev) return CreateRef<D3D12Texture2D>(dev, width, height);
			return nullptr;
		}
		case RendererAPI::API::Vulkan:
		{
			auto* ctx = Application::Get().GetWindow().GetGraphicsContext();
			auto* vkCtx = dynamic_cast<VulkanGraphicsContext*>(ctx);
			if (vkCtx) return CreateRef<VulkanTexture2D>(vkCtx->GetDevice(), width, height);
			return nullptr;
		}
		}

		CANDY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
	Ref<Texture2D> Texture2D::Create(const std::string& path)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:    CANDY_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
		case RendererAPI::API::OpenGL:  return CreateRef<OpenGLTexture2D>(path);
		case RendererAPI::API::D3D12:
		{
			auto* dev = GetD3D12Device();
			if (dev) return CreateRef<D3D12Texture2D>(dev, path);
			return nullptr;
		}
		case RendererAPI::API::Vulkan:
		{
			auto* ctx = Application::Get().GetWindow().GetGraphicsContext();
			auto* vkCtx = dynamic_cast<VulkanGraphicsContext*>(ctx);
			if (vkCtx) return CreateRef<VulkanTexture2D>(vkCtx->GetDevice(), path);
			return nullptr;
		}
		}

		CANDY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
