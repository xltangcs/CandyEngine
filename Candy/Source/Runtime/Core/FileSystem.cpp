#include "CandyPCH.h"
#include "Runtime/Core/FileSystem.h"
#include "Runtime/Core/PakFile.h"

namespace Candy {

	FileSystem& FileSystem::Get()
	{
		static FileSystem instance;
		return instance;
	}

	void FileSystem::Mount(const std::string& mountPoint, const std::filesystem::path& physicalPath)
	{
		MountPoint mp;
		mp.prefix = mountPoint;
		mp.path = physicalPath;

		if (physicalPath.has_extension() && physicalPath.extension() == ".pak")
		{
			mp.isPak = true;
			mp.pak = PakFile::Open(physicalPath);
			if (!mp.pak)
			{
				CANDY_CORE_ERROR("FileSystem: failed to mount pak: {0}", physicalPath.string());
				return;
			}
			CANDY_CORE_INFO("FileSystem: mounted pak '{0}' at '{1}'", physicalPath.string(), mountPoint);
		}
		else
		{
			mp.isPak = false;
			CANDY_CORE_INFO("FileSystem: mounted dir '{0}' at '{1}'", physicalPath.string(), mountPoint);
		}

		m_Mounts.push_back(mp);
	}

	void FileSystem::Unmount(const std::string& mountPoint)
	{
		m_Mounts.erase(
			std::remove_if(m_Mounts.begin(), m_Mounts.end(),
				[&](const MountPoint& mp) { return mp.prefix == mountPoint; }),
			m_Mounts.end());
	}

	const FileSystem::MountPoint* FileSystem::Resolve(const std::string& virtualPath, std::string& outRelativePath)
	{
		// Find the longest matching prefix (iterate in reverse for override priority)
		for (auto it = m_Mounts.rbegin(); it != m_Mounts.rend(); ++it)
		{
			const auto& mp = *it;
			if (virtualPath.compare(0, mp.prefix.size(), mp.prefix) == 0)
			{
				// Extract relative path after the prefix
				outRelativePath = virtualPath.substr(mp.prefix.size());
				// Strip leading slash
				if (!outRelativePath.empty() && outRelativePath[0] == '/')
					outRelativePath = outRelativePath.substr(1);
				return &mp;
			}
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

}
