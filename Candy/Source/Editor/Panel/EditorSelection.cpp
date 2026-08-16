#include "CandyPCH.h"
#include "EditorSelection.h"

#include <algorithm>
#include <cctype>

namespace Candy {

	EditorSelection& EditorSelection::Get()
	{
		static EditorSelection instance;
		return instance;
	}

	void EditorSelection::Clear()
	{
		m_SelectedEntity = {};
		m_SelectedAsset.clear();
		m_HasAssetSelection = false;
	}

	void EditorSelection::SelectEntity(Entity entity)
	{
		m_SelectedEntity = entity;
		m_SelectedAsset.clear();
		m_HasAssetSelection = false;
	}

	void EditorSelection::SelectAsset(const std::string& vfsPath)
	{
		m_SelectedEntity = {};
		m_SelectedAsset = vfsPath;
		m_HasAssetSelection = true;
	}

	std::string EditorSelection::GetExtension(const std::string& vfsPath)
	{
		// Find last '.' after last '/' (or start).
		auto lastSlash = vfsPath.find_last_of('/');
		auto lastDot   = vfsPath.find_last_of('.');
		if (lastDot == std::string::npos) return {};
		if (lastSlash != std::string::npos && lastDot < lastSlash) return {};
		if (lastDot + 1 >= vfsPath.size()) return {};
		return vfsPath.substr(lastDot);
	}

	bool EditorSelection::IsMaterial(const std::string& vfsPath)
	{
		std::string ext = GetExtension(vfsPath);
		std::string lower = ext;
		std::transform(lower.begin(), lower.end(), lower.begin(),
		               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return lower == ".mat";
	}

}
