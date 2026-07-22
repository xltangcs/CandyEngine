#pragma once

#include <string>
#include <optional>
#include <filesystem>

namespace Candy {

	std::string GetExecutableDirectory();
	std::filesystem::path GetExecutablePath();

	class FileDialogs
	{
	public:
		// These return empty strings if cancelled
		static std::string OpenFile(const char* filter);
		static std::string SaveFile(const char* filter);
		static std::string OpenFolder();
		static void OpenInShell(const std::string& path);
	};

}