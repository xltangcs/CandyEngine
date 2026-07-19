#pragma once

#include "Runtime/Core/Base.h"

#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <optional>

namespace Candy {

	class PakFile
	{
	public:
		struct Entry
		{
			std::string path;       // virtual path relative to pak root, e.g. "Shaders/Quad.glsl"
			uint64_t offset = 0;    // offset in the pak file
			uint64_t size = 0;      // uncompressed size
		};

		static Ref<PakFile> Open(const std::filesystem::path& pakPath);

		// Pack a directory into a .pak file using the same on-disk format as PakTool.
		// Returns true on success. The .pak is overwritten if it already exists.
		static bool Pack(const std::filesystem::path& inputDir, const std::filesystem::path& outputPak);

		bool HasFile(const std::string& relativePath) const;
		std::optional<std::vector<uint8_t>> ReadFile(const std::string& relativePath) const;

		const std::filesystem::path& GetPath() const { return m_PakPath; }

		// Access to all entries for directory enumeration (used by FileSystem::EnumerateDirectory).
		const std::unordered_map<std::string, Entry>& GetEntries() const { return m_Entries; }

	private:
		std::filesystem::path m_PakPath;
		std::unordered_map<std::string, Entry> m_Entries;
	};

}
