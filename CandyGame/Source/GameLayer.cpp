#include "GameLayer.h"

#include <algorithm>

#include "Runtime/Renderer/Renderer2D.h"
#include "Runtime/Renderer/RenderCommand.h"
#include "Runtime/Renderer/Framebuffer.h"
#include "Runtime/Renderer/GameFrameRenderer.h"
#include "Runtime/Project/Project.h"
#include "Runtime/Project/ProjectSettings.h"
#include "Runtime/Project/ProjectUtils.h"
#include "Runtime/Core/FileSystem.h"

#include <GLFW/glfw3.h>
#include <imgui/imgui.h>

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

		// DefaultScene is stored in VFS:// format.
		auto vfsScenePath = sceneName;
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
					auto w = window.GetWidth();
					auto h = window.GetHeight();
					m_ActiveScene->OnViewportResize(w, h);

					// Create SwapChainTarget framebuffer (binds to framebuffer 0)
					FramebufferSpecification fbSpec;
					fbSpec.Width = w;
					fbSpec.Height = h;
					fbSpec.SwapChainTarget = true;
					m_GameFramebuffer = Framebuffer::Create(fbSpec);

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
		if (!m_ActiveScene || !m_GameFramebuffer)
			return;

		// Resize framebuffer if window changed
		auto& window = Application::Get().GetWindow();
		uint32_t w = window.GetWidth();
		uint32_t h = window.GetHeight();
		auto& spec = m_GameFramebuffer->GetSpecification();
		if (spec.Width != w || spec.Height != h)
			m_GameFramebuffer->Resize(w, h);

		// Update logic (physics, scripts, audio)
		m_ActiveScene->OnUpdateRuntimeLogic(ts);

		// Render scene + UI into SwapChainTarget (framebuffer 0)
		// Clear first -- RenderSceneTo no longer clears, caller controls clear order.
		m_GameFramebuffer->Bind();
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		RenderCommand::Clear();
		GameFrameRenderer::RenderSceneTo(*m_GameFramebuffer, *m_ActiveScene, nullptr);

		// Mouse position in window coordinates (fullscreen game = 1:1)
		float mouseX = (float)Input::GetMouseX();
		float mouseY = (float)Input::GetMouseY();
		bool mouseDown = Input::IsMouseButtonPressed(Mouse::ButtonLeft);

		GameFrameRenderer::RenderUITo(*m_GameFramebuffer, *m_ActiveScene, mouseX, mouseY, mouseDown, ts.GetSeconds());

		m_GameFramebuffer->Unbind();
	}

	void GameLayer::OnEvent(Event& e)
	{
		// Handle window resize for SwapChainTarget framebuffer
		if (e.GetEventType() == EventType::WindowResize)
		{
			auto& re = (WindowResizeEvent&)e;
			if (m_GameFramebuffer)
				m_GameFramebuffer->Resize(re.GetWidth(), re.GetHeight());
			// Update scene cameras' aspect ratio -- without this, the scene renders with
			// the initial aspect and gets stretched when the window is resized.
			if (m_ActiveScene)
				m_ActiveScene->OnViewportResize(re.GetWidth(), re.GetHeight());
		}
	}

}
