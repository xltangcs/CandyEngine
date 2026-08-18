#include "CandyPCH.h"

#include "Runtime/Asset/ShaderCache.h"
#include "Runtime/Core/FileSystem.h"

namespace Candy {

	ShaderCache& ShaderCache::Get()
	{
		static ShaderCache instance;
		return instance;
	}

	const std::vector<ShaderParameter>* ShaderCache::GetParameters(const std::string& vfsPath)
	{
		if (vfsPath.empty())
			return nullptr;

		std::optional<std::string> text = FileSystem::Get().ReadText(vfsPath);
		if (!text)
		{
			// File unreadable: fall back to the cached list if one exists, so
			// temporarily missing files don't blank the inspector.
			auto it = m_Entries.find(vfsPath);
			return it != m_Entries.end() ? &it->second.Parameters : nullptr;
		}

		const uint64_t hash = std::hash<std::string>{}(*text);
		auto it = m_Entries.find(vfsPath);
		if (it != m_Entries.end() && it->second.SourceHash == hash)
			return &it->second.Parameters;

		Entry& entry = m_Entries[vfsPath];
		entry.SourceHash = hash;
		entry.Parameters = ShaderReflector::Reflect(*text);
		return &entry.Parameters;
	}

	void ShaderCache::Invalidate(const std::string& vfsPath)
	{
		if (vfsPath.empty())
			return;
		m_Entries.erase(vfsPath);
	}

	void ShaderCache::Clear()
	{
		m_Entries.clear();
	}

} // namespace Candy
