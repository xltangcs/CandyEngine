#pragma once

#include "Candy/Core/Base.h"
#include "Candy/Core/Log.h"
#include "Candy/Scene/Scene.h"
#include "Candy/Scene/Entity.h"

namespace Candy {

	class SceneHierarchyPanel
	{
	public:
		SceneHierarchyPanel() = default;
		SceneHierarchyPanel(const Ref<Scene>& scene);

		void SetContext(const Ref<Scene>& scene);

		void OnImGuiRender();
	private:
		void DrawEntityNode(Entity entity);
	private:
		Ref<Scene> m_Context;
		Entity m_SelectionContext;
	};

}