#pragma once

#include "Candy/Core/Base.h"

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

		bool HasFile(const std::string& relativePath) const;
		std::optional<std::vector<uint8_t>> ReadFile(const std::string& relativePath) const;

		const std::filesystem::path& GetPath() const { return m_PakPath; }

	private:
		std::filesystem::path m_PakPath;
		std::unordered_map<std::string, Entry> m_Entries;
	};

}
