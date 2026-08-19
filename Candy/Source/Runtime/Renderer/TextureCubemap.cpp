#include "CandyPCH.h"

#include "Runtime/Renderer/TextureCubemap.h"
#include "Runtime/Renderer/Renderer.h"
#include "Runtime/Core/Log.h"

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

		// Set by RegisterBackendFactory during backend init (composition root).
		TextureCubemap::CreateBackendFn s_BackendFactory = nullptr;

	}

	void TextureCubemap::RegisterBackendFactory(CreateBackendFn fn)
	{
		s_BackendFactory = fn;
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

		if (!s_BackendFactory)
		{
			CANDY_CORE_WARN("TextureCubemap: no backend factory registered for the active render API");
			return nullptr;
		}

		Ref<TextureCubemap> result = s_BackendFactory(vfsPath);
		if (!result || !result->IsLoaded())
			return nullptr;

		CubemapCache()[vfsPath] = result;
		return result;
	}

} // namespace Candy
