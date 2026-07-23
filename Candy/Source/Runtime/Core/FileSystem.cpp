#include "CandyPCH.h"
#include "Runtime/Core/FileSystem.h"
#include "Runtime/Core/PakFile.h"

namespace Candy {

	FileSystem& FileSystem::Get()
	{
		static FileSystem instance;
		return instance;
	}

	void FileSystem::Mount(const std::string& domain, const std::filesystem::path& physicalPath,
	                       const std::string& pakSubDir)
	{
		MountPoint mp;
		mp.prefix = domain;
		mp.path = physicalPath;
		mp.pakSubDir = pakSubDir;

		if (physicalPath.has_extension() && physicalPath.extension() == ".pak")
		{
			mp.isPak = true;
			mp.pak = PakFile::Open(physicalPath);
			if (!mp.pak)
			{
				CANDY_CORE_ERROR("FileSystem: failed to mount pak: {0}", physicalPath.string());
				return;
			}
			CANDY_CORE_INFO("FileSystem: mounted pak '{0}' at '{1}' (subdir '{2}')",
			                physicalPath.string(), domain, pakSubDir);
		}
		else
		{
			mp.isPak = false;
			CANDY_CORE_INFO("FileSystem: mounted dir '{0}' at '{1}'", physicalPath.string(), domain);
		}

		m_Mounts.push_back(mp);
	}

	void FileSystem::Unmount(const std::string& domain)
	{
		m_Mounts.erase(
			std::remove_if(m_Mounts.begin(), m_Mounts.end(),
				[&](const MountPoint& mp) { return mp.prefix == domain; }),
			m_Mounts.end());
	}

	const FileSystem::MountPoint* FileSystem::Resolve(const std::string& virtualPath, std::string& outRelativePath)
	{
		// Only accept VFS:// scheme.
		// "VFS://Engine/Icons/x.png" -> domain="Engine", relative="Icons/x.png"
		// "VFS://Game"               -> domain="Game",   relative=""
		const std::string scheme = "VFS://";
		if (!virtualPath.starts_with(scheme))
			return nullptr;

		std::string rest = virtualPath.substr(scheme.size());  // "Engine/Icons/x.png" or "Game"
		size_t slash = rest.find('/');
		std::string domain = (slash == std::string::npos) ? rest : rest.substr(0, slash);
		outRelativePath = (slash == std::string::npos) ? std::string() : rest.substr(slash + 1);

		// Iterate in reverse for override priority (last-mounted wins).
		for (auto it = m_Mounts.rbegin(); it != m_Mounts.rend(); ++it)
		{
			const auto& mp = *it;
			if (mp.prefix != domain)
				continue;

			// For standalone pak mounts, prepend the pak subdirectory so ReadFile
			// looks up "engine/Shaders/x.glsl" instead of "Shaders/x.glsl".
			if (mp.isPak && !mp.pakSubDir.empty())
			{
				if (outRelativePath.empty())
					outRelativePath = mp.pakSubDir;
				else
					outRelativePath = mp.pakSubDir + outRelativePath;
			}
			return &mp;
		}
		return nullptr;
	}

	std::optional<std::vector<uint8_t>> FileSystem::Read(const std::string& virtualPath)
	{
		std::string relPath;
		const MountPoint* mp = Resolve(virtualPath, relPath);
		if (!mp)
			return std::nullopt;

		if (mp->isPak)
		{
			return mp->pak->ReadFile(relPath);
		}
		else
		{
			std::filesystem::path fullPath = mp->path / relPath;
			if (!std::filesystem::exists(fullPath))
				return std::nullopt;

			std::ifstream file(fullPath, std::ios::binary);
			if (!file.is_open())
				return std::nullopt;

			file.seekg(0, std::ios::end);
			auto size = file.tellg();
			file.seekg(0, std::ios::beg);

			std::vector<uint8_t> data(size);
			file.read(reinterpret_cast<char*>(data.data()), size);
			return data;
		}
	}

	std::optional<std::string> FileSystem::ReadText(const std::string& virtualPath)
	{
		auto data = Read(virtualPath);
		if (!data)
			return std::nullopt;
		return std::string(data->begin(), data->end());
	}

	bool FileSystem::Exists(const std::string& virtualPath)
	{
		std::string relPath;
		const MountPoint* mp = Resolve(virtualPath, relPath);
		if (!mp)
			return false;

		if (mp->isPak)
		{
			return mp->pak->HasFile(relPath);
		}
		else
		{
			return std::filesystem::exists(mp->path / relPath);
		}
	}

	bool FileSystem::Write(const std::string& virtualPath, const std::vector<uint8_t>& data)
	{
		std::string relPath;
		const MountPoint* mp = Resolve(virtualPath, relPath);
		if (!mp || mp->isPak)
			return false;

		std::filesystem::path fullPath = mp->path / relPath;
		std::filesystem::create_directories(fullPath.parent_path());

		std::ofstream file(fullPath, std::ios::binary);
		if (!file.is_open())
			return false;

		file.write(reinterpret_cast<const char*>(data.data()), data.size());
		return true;
	}

	bool FileSystem::WriteText(const std::string& virtualPath, const std::string& text)
	{
		return Write(virtualPath, std::vector<uint8_t>(text.begin(), text.end()));
	}

	std::optional<std::filesystem::path> FileSystem::ToDiskPath(const std::string& virtualPath)
	{
		std::string relPath;
		const MountPoint* mp = Resolve(virtualPath, relPath);
		if (!mp || mp->isPak)
			return std::nullopt;

		std::filesystem::path fullPath = mp->path / relPath;
		return fullPath;
	}

	std::optional<std::filesystem::path> FileSystem::ResolveToDiskPath(const std::string& virtualPath)
	{
		// Only VFS:// paths are resolved at runtime.
		if (!virtualPath.starts_with("VFS://"))
			return std::nullopt;

		// Fast path: directory mounts already expose the real file on disk.
		auto disk = ToDiskPath(virtualPath);
		if (disk)
			return *disk;

		// Pak mounts: extract the file to a temp directory once, then reuse it.
		static std::unordered_map<std::string, std::filesystem::path> s_PakCache;
		auto it = s_PakCache.find(virtualPath);
		if (it != s_PakCache.end() && std::filesystem::exists(it->second))
			return it->second;

		auto data = Read(virtualPath);
		if (!data)
			return std::nullopt;

		VfsPath vp = VfsPath::Parse(virtualPath);
		if (!vp.IsValid())
			return std::nullopt;

		auto tempPath = std::filesystem::temp_directory_path() / "CandyGame" / vp.DomainLabel() / vp.relativePath;
		std::filesystem::create_directories(tempPath.parent_path());
		{
			std::ofstream out(tempPath, std::ios::binary);
			out.write(reinterpret_cast<const char*>(data->data()), static_cast<std::streamsize>(data->size()));
		}
		s_PakCache[virtualPath] = tempPath;
		CANDY_CORE_INFO("FileSystem: extracted to temp: {0}", tempPath.string());
		return tempPath;
	}

	std::vector<std::string> FileSystem::EnumerateDirectory(const std::string& virtualDir, bool recursive)
	{
		std::vector<std::string> result;

		// Parse the VFS path to get the domain — we need it for output construction
		// regardless of whether Resolve succeeds for a pak with subdir.
		VfsPath vp = VfsPath::Parse(virtualDir);
		if (!vp.IsValid())
			return result;

		// Resolve to find the mount point and the filesystem-level relative path.
		// For pak mounts, Resolve prepends pakSubDir to the returned relPath.
		std::string relPath;
		const MountPoint* mp = Resolve(virtualDir, relPath);
		if (!mp)
			return result;

		if (mp->isPak)
		{
			// Pak entries: filter by relPath prefix, then strip pakSubDir for VFS output.
			std::string prefix = relPath;
			if (!prefix.empty() && prefix.back() != '/')
				prefix += '/';
			size_t subDirLen = mp->pakSubDir.size();

			for (const auto& [entryPath, entry] : mp->pak->GetEntries())
			{
				if (!entryPath.starts_with(prefix))
					continue;
				std::string rest = entryPath.substr(prefix.size());
				if (!recursive && rest.find('/') != std::string::npos)
					continue;

				// Build the VFS-relative path (strip the pak subdirectory prefix).
				std::string vfsRel;
				if (!mp->pakSubDir.empty())
				{
					// entryPath = "engine/Shaders/x.glsl", pakSubDir = "engine/"
					// We want "Shaders/x.glsl" as the relative path for VFS://Engine/Shaders/x.glsl
					if (entryPath.starts_with(mp->pakSubDir))
						vfsRel = entryPath.substr(subDirLen);
					else
						vfsRel = entryPath;  // shouldn't happen, but be safe
				}
				else
				{
					vfsRel = entryPath;
				}

				// Include the current directory's relative part in the output.
				std::string fullRel = vp.relativePath.empty() ? vfsRel
				                              : vp.relativePath + "/" + vfsRel;
				result.push_back(VfsPath(vp.domain, fullRel).ToString());
			}
		}
		else
		{
			std::filesystem::path diskDir = mp->path / relPath;
			if (!std::filesystem::exists(diskDir))
				return result;

			if (recursive)
			{
				for (auto& entry : std::filesystem::recursive_directory_iterator(diskDir))
				{
					std::string rel = std::filesystem::relative(entry.path(), mp->path).generic_string();
					result.push_back(VfsPath(vp.domain, rel).ToString());
				}
			}
			else
			{
				for (auto& entry : std::filesystem::directory_iterator(diskDir))
				{
					std::string rel = std::filesystem::relative(entry.path(), mp->path).generic_string();
					result.push_back(VfsPath(vp.domain, rel).ToString());
				}
			}
		}
		return result;
	}

}
