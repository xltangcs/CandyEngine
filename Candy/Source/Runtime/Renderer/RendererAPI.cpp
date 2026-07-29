#include "CandyPCH.h"

#include "Runtime/Renderer/RendererAPI.h"
#include "Platform/OpenGL/OpenGLRendererAPI.h"
#include "Platform/DX12/DX12RendererAPI.h"
#include "Platform/Vulkan/VulkanRendererAPI.h"

namespace Candy {
	RendererAPI::API RendererAPI::s_API = RendererAPI::API::OpenGL;

	Scope<RendererAPI> RendererAPI::Create()
	{
		switch (s_API)
		{
		case RendererAPI::API::None:    CANDY_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
		case RendererAPI::API::OpenGL:  return CreateScope<OpenGLRendererAPI>();
		case RendererAPI::API::DX12:    return CreateScope<DX12RendererAPI>();
		case RendererAPI::API::Vulkan:  return CreateScope<VulkanRendererAPI>();
		}

		CANDY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}