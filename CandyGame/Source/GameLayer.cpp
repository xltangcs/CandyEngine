#include "GameLayer.h"

#include <algorithm>

#include "Candy/Renderer/Renderer2D.h"
#include "Candy/Renderer/RenderCommand.h"
#include "Candy/Project/Project.h"
#include "Candy/Project/ProjectSettings.h"
#include "Candy/Project/ProjectUtils.h"
#include "Candy/Core/FileSystem.h"

namespace Candy {

	GameLayer::GameLayer()
		: Layer("GameLayer")
	{
	}

	void GameLayer::OnAttach()
	{
		auto project = Application::Get().GetProject();
		if (!project)
		{
			CANDY_CORE_ERROR("No project loaded");
			return;
		}

		m_ActiveScene = CreateRef<Scene>();

		auto sceneName = ProjectSettings::Get().DefaultScene;
		if (sceneName.empty())
		{
			CANDY_CORE_ERROR("No DefaultScene set in Config/ProjectSetting.candy");
			return;
		}

		// Normalize to forward slashes for VFS path matching
		std::replace(sceneName.begin(), sceneName.end(), '\\', '/');

		auto vfsScenePath = "/project/" + sceneName;
		if (FileSystem::Get().Exists(vfsScenePath))
		{
			auto yamlContent = FileSystem::Get().ReadText(vfsScenePath);
			if (yamlContent)
			{
				SceneSerializer serializer(m_ActiveScene);
				if (serializer.DeserializeFromString(*yamlContent))
				{
					CANDY_CORE_INFO("Loaded scene from VFS: {0}", vfsScenePath);
					auto& window = Application::Get().GetWindow();
					m_ActiveScene->OnViewportResize(window.GetWidth(), window.GetHeight());
					m_ActiveScene->OnRuntimeStart();
					return;
				}
			}
		}

		CANDY_CORE_ERROR("Scene not found in VFS: {0}", vfsScenePath);
	}

	void GameLayer::OnDetach()
	{
		if (m_ActiveScene)
			m_ActiveScene->OnRuntimeStop();
	}

	void GameLayer::OnUpdate(Timestep ts)
	{
		if (!m_ActiveScene)
			return;

		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		RenderCommand::Clear();

		m_ActiveScene->OnUpdateRuntime(ts);
	}

	void GameLayer::OnEvent(Event& e)
	{
	}

}
