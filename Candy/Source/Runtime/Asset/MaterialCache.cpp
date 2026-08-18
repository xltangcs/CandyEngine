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
		// The file on disk may have changed; invalidate GPU-side state consumers.
		m_Revisions[vfsPath]++;
	}

	void MaterialCache::Clear()
	{
		m_Materials.clear();
		m_Revisions.clear();
	}

	void MaterialCache::Touch(const std::string& vfsPath)
	{
		if (vfsPath.empty())
			return;
		m_Revisions[vfsPath]++;
	}

	uint64_t MaterialCache::GetRevision(const std::string& vfsPath) const
	{
		auto it = m_Revisions.find(vfsPath);
		return it != m_Revisions.end() ? it->second : 0;
	}

}
