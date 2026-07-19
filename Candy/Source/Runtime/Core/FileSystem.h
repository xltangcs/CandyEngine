#pragma once

#include "Runtime/Core/Base.h"

#include <filesystem>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

namespace Candy {

	class PakFile;

	class FileSystem
	{
	public:
		static FileSystem& Get();

		// Mount a physical path (directory or .pak file) at a virtual mount point
		// e.g. Mount("/engine", "Candy/Content/")
		//      Mount("/project", "MyGame.pak")
		void Mount(const std::string& mountPoint, const std::filesystem::path& physicalPath);

		// Unmount a mount point
		void Unmount(const std::string& mountPoint);

		// Read entire file as bytes. Returns empty optional if not found.
		std::optional<std::vector<uint8_t>> Read(const std::string& virtualPath);

		// Read entire file as string. Returns empty optional if not found.
		std::optional<std::string> ReadText(const std::string& virtualPath);

		// Check if a virtual file exists
		bool Exists(const std::string& virtualPath);

		// Write file to physical path (always disk, not pak)
		bool Write(const std::string& virtualPath, const std::vector<uint8_t>& data);
		bool WriteText(const std::string& virtualPath, const std::string& text);

	private:
		FileSystem() = default;

		struct MountPoint
		{
			std::string prefix;           // e.g. "/engine", "/project"
			std::filesystem::path path;   // physical path
			bool isPak = false;
			Ref<PakFile> pak;             // valid if isPak
		};

		std::vector<MountPoint> m_Mounts;

		// Resolve a virtual path to a mount point + relative path
		const MountPoint* Resolve(const std::string& virtualPath, std::string& outRelativePath);
	};

}
