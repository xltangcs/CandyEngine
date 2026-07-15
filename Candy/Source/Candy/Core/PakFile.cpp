#include "CandyPCH.h"
#include "Candy/Core/PakFile.h"

namespace Candy {

	static constexpr uint32_t PAK_MAGIC = 0x4B415000; // 'PAK\0'
	static constexpr uint32_t PAK_VERSION = 1;

	Ref<PakFile> PakFile::Open(const std::filesystem::path& pakPath)
	{
		std::ifstream file(pakPath, std::ios::binary);
		if (!file.is_open())
		{
			CANDY_CORE_ERROR("Failed to open pak file: {0}", pakPath.string());
			return nullptr;
		}

		uint32_t magic = 0, version = 0, entryCount = 0;
		file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
		file.read(reinterpret_cast<char*>(&version), sizeof(version));
		file.read(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));

		if (magic != PAK_MAGIC)
		{
			CANDY_CORE_ERROR("Invalid pak magic: 0x{0:08X}", magic);
			return nullptr;
		}

		if (version != PAK_VERSION)
		{
			CANDY_CORE_ERROR("Unsupported pak version: {0}", version);
			return nullptr;
		}

		auto pak = CreateRef<PakFile>();
		pak->m_PakPath = pakPath;

		for (uint32_t i = 0; i < entryCount; i++)
		{
			Entry entry;
			uint32_t pathLen = 0;
			file.read(reinterpret_cast<char*>(&pathLen), sizeof(pathLen));
			entry.path.resize(pathLen);
			file.read(entry.path.data(), pathLen);
			file.read(reinterpret_cast<char*>(&entry.offset), sizeof(entry.offset));
			file.read(reinterpret_cast<char*>(&entry.size), sizeof(entry.size));

			pak->m_Entries[entry.path] = entry;
		}

		return pak;
	}

	bool PakFile::HasFile(const std::string& relativePath) const
	{
		return m_Entries.find(relativePath) != m_Entries.end();
	}

	std::optional<std::vector<uint8_t>> PakFile::ReadFile(const std::string& relativePath) const
	{
		auto it = m_Entries.find(relativePath);
		if (it == m_Entries.end())
			return std::nullopt;

		std::ifstream file(m_PakPath, std::ios::binary);
		if (!file.is_open())
			return std::nullopt;

		file.seekg(it->second.offset);
		std::vector<uint8_t> data(it->second.size);
		file.read(reinterpret_cast<char*>(data.data()), it->second.size);

		return data;
	}

}
