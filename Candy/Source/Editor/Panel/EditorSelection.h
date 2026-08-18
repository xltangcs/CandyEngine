#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/Core/VfsPath.h"
#include "Runtime/Scene/Entity.h"

namespace Candy {

	// =========================================================================
	// EditorSelection — global editor selection state.
	//
	// Holds the "currently selected thing" in the editor. Either:
	//   * An entity (m_SelectedEntity) when the click came from the Scene Hierarchy
	//   * A VFS asset path (m_SelectedAsset) when the click came from the Content Browser
	//
	// Exactly one of these is "active" at any time. The flag m_HasAssetSelection
	// disambiguates between "no entity selected" and "asset selected".
	//
	// Panels (SceneHierarchyPanel, ContentBrowserPanel) both read/write this
	// single source of truth so the Properties window can render entity
	// components OR an asset inspector depending on what was clicked.
	// =========================================================================
	class EditorSelection
	{
	public:
		static EditorSelection& Get();

		// Clear selection (entity + asset both cleared).
		void Clear();

		// Select an entity. Clears any asset selection.
		void SelectEntity(Entity entity);

		// Select an asset by VFS path. Clears any entity selection.
		void SelectAsset(const std::string& vfsPath);

		bool HasEntitySelection() const { return m_SelectedEntity && !m_HasAssetSelection; }
		bool HasAssetSelection()  const { return m_HasAssetSelection; }
		bool IsEmpty()            const { return !m_SelectedEntity && !m_HasAssetSelection; }

		Entity                GetSelectedEntity() const { return m_SelectedEntity; }
		const std::string&    GetSelectedAsset()  const { return m_SelectedAsset; }

		// File extension helpers.
		static std::string GetExtension(const std::string& vfsPath);
		static bool IsMaterial(const std::string& vfsPath);
		static bool IsShader(const std::string& vfsPath);

	private:
		EditorSelection() = default;

		Entity         m_SelectedEntity;
		std::string    m_SelectedAsset;   // "VFS://Game/Materials/Default.mat"
		bool           m_HasAssetSelection = false;
	};

}
