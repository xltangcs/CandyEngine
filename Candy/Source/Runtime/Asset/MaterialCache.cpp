#include "CandyPCH.h"
#include "Runtime/Asset/MaterialCache.h"

namespace Candy {

	MaterialCache& MaterialCache::Get()
	{
		static MaterialCache instance;
		return instance;
	}

	Ref<Material> MaterialCache::Load(const std::string& vfsPath)
	{
		if (vfsPath.empty())
			return nullptr;

		auto it = m_Materials.find(vfsPath);
		if (it != m_Materials.end())
			return it->second;

		Ref<Material> material = Material::Load(vfsPath);
		if (material)
			m_Materials[vfsPath] = material;

		return material;
	}

	void MaterialCache::Reload(const std::string& vfsPath)
	{
		if (vfsPath.empty())
			return;

		m_Materials.erase(vfsPath);
	}

	void MaterialCache::Clear()
	{
		m_Materials.clear();
	}

}
