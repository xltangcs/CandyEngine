#include "CandyPCH.h"

#include "Runtime/Renderer/RendererAPI.h"
#include "Platform/OpenGL/OpenGLRendererAPI.h"
#include "Platform/D3D12/D3D12RendererAPI.h"
#include "Platform/Vulkan/VulkanRendererAPI.h"

namespace Candy {
	// Default backend is D3D12.  The value is also mirrored into per-project
	// .candyproj files via `Project::GetRendererAPI()` so projects can override
	// on a per-project basis.  Use RendererAPI::SetAPI(...) before Window /
	// GraphicsContext creation to pick the active backend.
	RendererAPI::API RendererAPI::s_API = RendererAPI::API::D3D12;

	RendererAPI::API RendererAPI::APIFromString(const std::string& str)
	{
		if (str == "OpenGL" || str == "opengl") return API::OpenGL;
		if (str == "Vulkan" || str == "vulkan") return API::Vulkan;
		if (str == "D3D12" || str == "d3d12" || str == "DX12" || str == "dx12")
			return API::D3D12;
		return API::OpenGL;
	}

	const char* RendererAPI::StringFromAPI(API api)
	{
		switch (api)
		{
		case API::OpenGL: return "OpenGL";
		case API::Vulkan: return "Vulkan";
		case API::D3D12:   return "D3D12";
		case API::None:    return "None";
		}
		return "Unknown";
	}

	Scope<RendererAPI> RendererAPI::Create()
	{
		switch (s_API)
		{
		case RendererAPI::API::None:    CANDY_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
		case RendererAPI::API::OpenGL:  return CreateScope<OpenGLRendererAPI>();
		case RendererAPI::API::D3D12:   return CreateScope<D3D12RendererAPI>();
		case RendererAPI::API::Vulkan:  return CreateScope<VulkanRendererAPI>();
		}

		CANDY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
