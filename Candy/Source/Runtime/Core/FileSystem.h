#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/Core/VfsPath.h"

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

		// A mount point maps a VFS domain ("Engine" or "Game") to a physical
		// directory or .pak file on disk.
		struct MountPoint
		{
			std::string prefix;              // "Engine" or "Game"
			std::filesystem::path path;      // physical path (directory or .pak)
			bool isPak = false;
			Ref<PakFile> pak;                // valid if isPak
			std::string pakSubDir;           // subdirectory within the pak, e.g. "engine/" (standalone mode only)
		};

		// Mount a physical path at a VFS domain. When mounting a single .pak that
		// contains both engine/ and game/ subdirectories (standalone mode), use
		// pakSubDir to specify the subdirectory within the pak.
		// Examples:
		//   Mount("Engine", "Candy/Content/")              — editor: directory
		//   Mount("Game",   "MyGame.pak")                  — game: pak w/o subdir
		//   Mount("Engine", "Standalone.pak", "engine/")   — standalone: subdir
		//   Mount("Game",   "Standalone.pak", "game/")     — standalone: subdir
		void Mount(const std::string& domain, const std::filesystem::path& physicalPath,
		           const std::string& pakSubDir = "");

		// Unmount a domain
		void Unmount(const std::string& domain);

		// Read entire file as bytes. Returns empty optional if not found.
		// Only accepts VFS:// scheme paths.
		std::optional<std::vector<uint8_t>> Read(const std::string& virtualPath);

		// Read entire file as string. Returns empty optional if not found.
		std::optional<std::string> ReadText(const std::string& virtualPath);

		// Check if a virtual file exists
		bool Exists(const std::string& virtualPath);

		// Write file to physical path (always disk, not pak)
		bool Write(const std::string& virtualPath, const std::vector<uint8_t>& data);
		bool WriteText(const std::string& virtualPath, const std::string& text);

		// Resolve a VFS:// path to an absolute disk path. Returns nullopt for
		// pak-only mounts or unresolvable paths.
		std::optional<std::filesystem::path> ToDiskPath(const std::string& virtualPath);
		std::optional<std::filesystem::path> ToDiskPath(const VfsPath& p) { return ToDiskPath(p.ToString()); }

		// Resolve a VFS:// path to a real file on disk.
		//   - Directory mounts return the actual on-disk path (fast path).
		//   - Pak mounts extract the file to a temp directory once, then reuse it.
		// Returns nullopt for non-VFS paths or if the file cannot be resolved.
		// Legacy / disk paths are the concern of the serialization layer
		// (MigrateLegacyPath), not of callers requesting a real file.
		std::optional<std::filesystem::path> ResolveToDiskPath(const std::string& virtualPath);
		std::optional<std::filesystem::path> ResolveToDiskPath(const VfsPath& p) { return ResolveToDiskPath(p.ToString()); }

		// Enumerate entries directly under virtualDir (or recursively if recursive=true).
		// Returns paths in VFS:// format. For pak mounts, lists entries whose path
		// starts with the directory prefix (taking pakSubDir into account).
		std::vector<std::string> EnumerateDirectory(const std::string& virtualDir, bool recursive = false);

		// Resolve a VFS:// path to a mount point + relative path (relative to the
		// mount point root, with pakSubDir prepended for pak mounts).
		const MountPoint* Resolve(const std::string& virtualPath, std::string& outRelativePath);

	private:
		FileSystem() = default;

		std::vector<MountPoint> m_Mounts;
	};

}
