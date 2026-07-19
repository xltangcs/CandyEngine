#pragma once

#include <string>
#include <optional>

namespace Candy {

	std::string GetExecutableDirectory();

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