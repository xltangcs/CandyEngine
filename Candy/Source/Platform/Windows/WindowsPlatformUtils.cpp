#include "CandyPCH.h"

#include "Runtime/Utils/PlatformUtils.h"

#include <commdlg.h>
#include <shellapi.h>
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "Runtime/Core/Application.h"

namespace Candy {

	std::string FileDialogs::OpenFile(const char* filter)
	{
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };
		CHAR currentDir[256] = { 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		if (GetCurrentDirectoryA(256, currentDir))
			ofn.lpstrInitialDir = currentDir;
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;

		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
		if (GetOpenFileNameA(&ofn) == TRUE)
		{
			return ofn.lpstrFile;
		}
		return std::string();
	}

	std::string FileDialogs::SaveFile(const char* filter)
	{
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };
		CHAR currentDir[256] = { 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		if (GetCurrentDirectoryA(256, currentDir))
			ofn.lpstrInitialDir = currentDir;
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
		// Sets the default extension by extracting it from the filter
		ofn.lpstrDefExt = strchr(filter, '\0') + 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
		if (GetSaveFileNameA(&ofn) == TRUE)
		{
			return ofn.lpstrFile;
		}
		return std::string();
	}

	std::string FileDialogs::OpenFolder()
	{
		IFileOpenDialog* pFileOpen = nullptr;
		HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
			IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
		if (FAILED(hr))
			return std::string();

		DWORD dwOptions;
		pFileOpen->GetOptions(&dwOptions);
		pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);

		hr = pFileOpen->Show(nullptr);
		std::string result;
		if (SUCCEEDED(hr))
		{
			IShellItem* pItem;
			hr = pFileOpen->GetResult(&pItem);
			if (SUCCEEDED(hr))
			{
				PWSTR pszPath;
				hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
				if (SUCCEEDED(hr))
				{
					int size = WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, nullptr, 0, nullptr, nullptr);
					if (size > 0)
					{
						result.resize(size - 1);
						WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, &result[0], size, nullptr, nullptr);
					}
					CoTaskMemFree(pszPath);
				}
				pItem->Release();
			}
		}
		pFileOpen->Release();
		return result;
	}

	void FileDialogs::OpenInShell(const std::string& path)
	{
		ShellExecuteA(nullptr, "explore", path.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
	}

	std::filesystem::path GetExecutablePath()
	{
		wchar_t buf[MAX_PATH];
		GetModuleFileNameW(nullptr, buf, MAX_PATH);
		return std::filesystem::path(buf);
	}

	std::string GetExecutableDirectory()
	{
		return GetExecutablePath().parent_path().string();
	}

}