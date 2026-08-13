#include "CandyPCH.h"

#include "Runtime/Renderer/Texture.h"
#include "Runtime/Renderer/Renderer.h"

#include "Runtime/RHI/RHIContext.h"
#include "Runtime/RHI/RHIDevice.h"

#include "Platform/OpenGL/OpenGLTexture.h"
#include "Platform/D3D12/D3D12Device.h"
#include "Platform/D3D12/D3D12Texture2D.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanTexture2D.h"

namespace Candy {

	// Engine-level Texture2D subclasses are created here (they carry file
	// loading + ImGui SRV concerns that don't belong in the RHI layer); the
	// active backend's device comes from RHIContext — no GraphicsContext
	// downcasts needed.
	static D3D12Device* GetD3D12Device()
	{
		return static_cast<D3D12Device*>(RHIContext::GetDevice());
	}

	static VulkanDevice* GetVulkanDevice()
	{
		return static_cast<VulkanDevice*>(RHIContext::GetDevice());
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
			auto* dev = GetVulkanDevice();
			if (dev) return CreateRef<VulkanTexture2D>(dev, width, height);
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
			auto* dev = GetVulkanDevice();
			if (dev) return CreateRef<VulkanTexture2D>(dev, path);
			return nullptr;
		}
		}

		CANDY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
