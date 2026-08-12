#include "CandyPCH.h"

#include "Runtime/Renderer/GraphicsContext.h"

#include "Runtime/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLContext.h"
#include "Platform/D3D12/D3D12GraphicsContext.h"
#include "Platform/Vulkan/VulkanGraphicsContext.h"

namespace Candy {

	Scope<GraphicsContext> GraphicsContext::Create(const WindowHandle& handle)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:    CANDY_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
		case RendererAPI::API::OpenGL:  return CreateScope<OpenGLContext>(handle);
		case RendererAPI::API::D3D12:    return CreateScope<D3D12GraphicsContext>(handle);
		case RendererAPI::API::Vulkan:  return CreateScope<VulkanGraphicsContext>(handle);
		}

		CANDY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}