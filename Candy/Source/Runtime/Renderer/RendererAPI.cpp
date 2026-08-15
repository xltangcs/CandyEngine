#include "CandyPCH.h"

#include "Runtime/Renderer/RendererAPI.h"

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
		if (str == "D3D12" || str == "d3d12")	return API::D3D12;
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
}
