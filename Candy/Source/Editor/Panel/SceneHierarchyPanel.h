#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/Scene/Scene.h"
#include "Runtime/Scene/Entity.h"

#include <functional>

namespace Candy {

	class SceneHierarchyPanel
	{
	public:
		SceneHierarchyPanel() = default;
		SceneHierarchyPanel(const Ref<Scene>& scene);

		void SetContext(const Ref<Scene>& scene);

		void SetEntityDoubleClickedCallback(const std::function<void(Entity)>& callback) { m_OnEntityDoubleClicked = callback; }

		void OnImGuiRender();
		Entity GetSelectedEntity() const { return m_SelectionContext; }
		void SetSelectedEntity(Entity entity);
	private:
		bool DrawEntityNode(Entity entity);
		void DrawComponents(Entity entity);
		void DrawSelectedAsset(const std::string& vfsPath);
	private:
		Ref<Scene> m_Context;
		Entity m_SelectionContext;
		std::function<void(Entity)> m_OnEntityDoubleClicked;
	};

}