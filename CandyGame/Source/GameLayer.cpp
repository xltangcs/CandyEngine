#include "GameLayer.h"

#include "Candy/Renderer/Renderer2D.h"
#include "Candy/Renderer/RenderCommand.h"
#include "Candy/Project/Project.h"
#include "Candy/Project/ProjectUtils.h"

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

		auto scenePath = project->GetFullStartScenePath();
		if (std::filesystem::exists(scenePath))
		{
			SceneSerializer serializer(m_ActiveScene);
			if (serializer.Deserialize(scenePath.string()))
			{
				CANDY_CORE_INFO("Loaded scene: {0}", scenePath.string());
			}
			else
			{
				CANDY_CORE_ERROR("Failed to load scene: {0}", scenePath.string());
			}
		}
		else
		{
			CANDY_CORE_WARN("Start scene not found: {0}", scenePath.string());
		}

		m_ActiveScene->OnRuntimeStart();
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
