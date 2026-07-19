#include "CandyPCH.h"
#include "Runtime/Core/PakFile.h"

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

	bool PakFile::Pack(const std::filesystem::path& inputDir, const std::filesystem::path& outputPak)
	{
		if (!std::filesystem::is_directory(inputDir))
		{
			CANDY_CORE_ERROR("PakFile::Pack failed: input directory does not exist: {0}", inputDir.string());
			return false;
		}

		// Collect files relative to inputDir
		struct Entry
		{
			std::string path;
			uint64_t size = 0;
		};
		std::vector<Entry> entries;
		for (auto& e : std::filesystem::recursive_directory_iterator(inputDir))
		{
			if (!e.is_regular_file())
				continue;
			Entry entry;
			entry.path = std::filesystem::relative(e.path(), inputDir).generic_string();
			entry.size = std::filesystem::file_size(e.path());
			entries.push_back(entry);
		}

		uint32_t entryCount = static_cast<uint32_t>(entries.size());

		// Layout: header (MAGIC + VERSION + entryCount) + entry table + file data
		uint64_t headerSize = sizeof(PAK_MAGIC) + sizeof(PAK_VERSION) + sizeof(entryCount);
		uint64_t entryTableSize = 0;
		for (auto& e : entries)
		{
			entryTableSize += sizeof(uint32_t); // pathLen
			entryTableSize += e.path.size();    // path
			entryTableSize += sizeof(uint64_t); // offset
			entryTableSize += sizeof(uint64_t); // size
		}

		uint64_t dataOffset = headerSize + entryTableSize;
		uint64_t currentOffset = dataOffset;
		std::vector<uint64_t> offsets(entries.size());
		for (size_t i = 0; i < entries.size(); i++)
		{
			offsets[i] = currentOffset;
			currentOffset += entries[i].size;
		}

		std::ofstream out(outputPak, std::ios::binary | std::ios::trunc);
		if (!out.is_open())
		{
			CANDY_CORE_ERROR("PakFile::Pack failed: cannot create output file: {0}", outputPak.string());
			return false;
		}

		// Header
		out.write(reinterpret_cast<const char*>(&PAK_MAGIC), sizeof(PAK_MAGIC));
		out.write(reinterpret_cast<const char*>(&PAK_VERSION), sizeof(PAK_VERSION));
		out.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));

		// Entry table
		for (size_t i = 0; i < entries.size(); i++)
		{
			uint32_t pathLen = static_cast<uint32_t>(entries[i].path.size());
			out.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
			out.write(entries[i].path.data(), pathLen);
			out.write(reinterpret_cast<const char*>(&offsets[i]), sizeof(uint64_t));
			out.write(reinterpret_cast<const char*>(&entries[i].size), sizeof(uint64_t));
		}

		// File data
		for (size_t i = 0; i < entries.size(); i++)
		{
			std::ifstream in(inputDir / entries[i].path, std::ios::binary);
			if (!in.is_open())
			{
				CANDY_CORE_ERROR("PakFile::Pack failed: cannot read file: {0}", (inputDir / entries[i].path).string());
				return false;
			}
			out << in.rdbuf();
		}

		out.close();
		CANDY_CORE_INFO("PakFile::Pack packed {0} files into {1} ({2} bytes)", entryCount, outputPak.string(), currentOffset);
		return true;
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
