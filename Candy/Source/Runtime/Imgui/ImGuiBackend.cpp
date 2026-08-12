#include "CandyPCH.h"

#include "Runtime/Imgui/ImGuiBackend.h"
#include "Runtime/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLImGuiBackend.h"
#ifdef CANDY_PLATFORM_WINDOWS
#include "Platform/D3D12/D3D12ImGuiBackend.h"
#include "Platform/Vulkan/VulkanImGuiBackend.h"
#endif

namespace Candy {

	Scope<ImGuiBackend> ImGuiBackend::Create()
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::OpenGL:
			return CreateScope<OpenGLImGuiBackend>();
#ifdef CANDY_PLATFORM_WINDOWS
		case RendererAPI::API::D3D12:
			return CreateScope<D3D12ImGuiBackend>();
		case RendererAPI::API::Vulkan:
			return CreateScope<VulkanImGuiBackend>();
#endif
		default:
			CANDY_CORE_ASSERT(false, "ImGuiBackend::Create — unsupported renderer API");
			return nullptr;
		}
	}
}
