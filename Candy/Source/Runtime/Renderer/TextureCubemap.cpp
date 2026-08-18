#include "CandyPCH.h"

#include "Runtime/Renderer/TextureCubemap.h"
#include "Runtime/Renderer/Renderer.h"
#include "Runtime/Core/Log.h"
#include "Runtime/RHI/RHIContext.h"

#include "Platform/D3D12/D3D12Device.h"
#include "Platform/D3D12/D3D12TextureCubemap.h"

#include <unordered_map>

namespace Candy {

	namespace {

		// Path -> weak cache: baking is expensive, so a given panorama is
		// converted + prefiltered exactly once per session.
		std::unordered_map<std::string, std::weak_ptr<TextureCubemap>>& CubemapCache()
		{
			static std::unordered_map<std::string, std::weak_ptr<TextureCubemap>> s_Cache;
			return s_Cache;
		}

	}

	Ref<TextureCubemap> TextureCubemap::CreateFromEquirect(const std::string& vfsPath)
	{
		if (vfsPath.empty())
			return nullptr;

		{
			auto it = CubemapCache().find(vfsPath);
			if (it != CubemapCache().end())
			{
				if (auto cached = it->second.lock())
					return cached;
			}
		}

		Ref<TextureCubemap> result;
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			CANDY_CORE_ASSERT(false, "RendererAPI::None is currently not supported!");
			return nullptr;
		case RendererAPI::API::OpenGL:
			CANDY_CORE_WARN("TextureCubemap: cubemaps are only supported on the D3D12 backend for now (OpenGL)");
			return nullptr;
		case RendererAPI::API::D3D12:
		{
			auto* dev = static_cast<D3D12Device*>(RHIContext::GetDevice());
			if (dev)
				result = CreateRef<D3D12TextureCubemap>(dev, vfsPath);
			break;
		}
		}

		if (!result || !result->IsLoaded())
			return nullptr;

		CubemapCache()[vfsPath] = result;
		return result;
	}

} // namespace Candy
