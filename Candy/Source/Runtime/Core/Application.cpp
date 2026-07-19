#include "CandyPCH.h"
#include "Runtime/Core/Application.h"

#include "Runtime/Core/Log.h"
#include "Runtime/Core/Input.h"
#include "Runtime/Core/FileSystem.h"
#include "Runtime/Renderer/Renderer.h"
#include "Runtime/Project/ProjectSerializer.h"
#include "Runtime/Project/ProjectSettings.h"

#include <glfw/glfw3.h>

#include "Runtime/Scripting/ScriptSystem.h"

namespace Candy {
	Application* Application::s_Instance = nullptr;

	Application::Application(const std::string& name, uint32_t width, uint32_t height, bool resizable, bool isEditor)
		: m_IsEditor(isEditor)
	{
		CANDY_CORE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;
		m_Window = Window::Create(WindowProps(name, width, height, resizable));
		m_Window->SetEventCallback(CANDY_BIND_EVENT_FN(Application::OnEvent));

		// Mount engine content: Editor → source directory, Game → .pak
		if (m_IsEditor)
		{
			auto engineDir = std::filesystem::path("..") / "Candy" / "Content";
			if (!std::filesystem::exists(engineDir))
				engineDir = "Content";
			if (std::filesystem::exists(engineDir))
				FileSystem::Get().Mount("Engine", engineDir);
			else
				CANDY_CORE_WARN("Engine content not found: {0}", engineDir.string());
		}
		else
		{
			auto pakPath = std::filesystem::path("Content.pak");
			if (std::filesystem::exists(pakPath))
				FileSystem::Get().Mount("Engine", pakPath);
		}

		Renderer::Init();
		AudioEngine::Init();
		ScriptSystem::Get().InitPython();

		m_ImGuiLayer = new ImGuiLayer();
		PushOverlay(m_ImGuiLayer);

		ImGuiLayer::RebuildFont(m_FontPath);
	}

	Application::~Application()
	{
		m_LayerStack.Clear();
		AudioEngine::Shutdown();
		Renderer::Shutdown();
		ScriptSystem::Get().ShutdownPython();
	}

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
	}

	void Application::PushOverlay(Layer* layer)
	{
		m_LayerStack.PushOverlay(layer);
	}

	void Application::PopLayer(Layer* layer)
	{
		m_LayerStack.PopLayer(layer);
	}

	void Application::PopOverlay(Layer* overlay)
	{
		m_LayerStack.PopOverlay(overlay);
	}

	void Application::Close()
	{
		m_Running = false;
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent> (CANDY_BIND_EVENT_FN(Application::OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(CANDY_BIND_EVENT_FN(Application::OnWindowResize));

		for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
		{
			if (e.Handled)
				break;
			(*it)->OnEvent(e);
		}
	}

	void Application::Run()
	{
		while (m_Running)
		{
			float time = (float)glfwGetTime();
			Timestep timestep = time - m_LastFrameTime;
			m_LastFrameTime = time;
			
			//update layer
			if (!m_Minimized)
			{
				for (Layer* layer : m_LayerStack)
					layer->OnUpdate(timestep);
			}

			m_ImGuiLayer->Begin();
			for (Layer* layer : m_LayerStack)
				layer->OnImGuiRender();
			m_ImGuiLayer->End();

			m_Window->OnUpdate();

			ProcessScheduledLayerChanges();
		}
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		if (e.GetWidth() == 0 || e.GetHeight() == 0)
		{
			m_Minimized = true;
			return false;
		}

		m_Minimized = false;
		Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());
		CANDY_CORE_TRACE("Resize Window size to ({0}, {1})", e.GetWidth(), e.GetHeight());

		return false;
	}

	void Application::LoadProject(const std::filesystem::path& projectFile)
	{
		auto project = Project::Load(projectFile);
		if (project)
		{
			m_ActiveProject = project;
			UpdateWindowTitle();

			// Mount project content: prefer a packed .pak, fall back to the directory
			auto contentDir = project->GetProjectDirectory() / "Content";
			auto contentPak = project->GetProjectDirectory() / "Content.pak";
			if (m_IsEditor)
			{
				if (std::filesystem::exists(contentDir))
					FileSystem::Get().Mount("Game", contentDir);
				else if (std::filesystem::exists(contentPak))
					FileSystem::Get().Mount("Game", contentPak);
			}
			else
			{
				if (std::filesystem::exists(contentPak))
					FileSystem::Get().Mount("Game", contentPak);
				else if (std::filesystem::exists(contentDir))
					FileSystem::Get().Mount("Game", contentDir);
			}
		}
	}

	void Application::LoadProjectFromVfs(const std::string& vfsProjectPath)
	{
		auto yamlContent = FileSystem::Get().ReadText(vfsProjectPath);
		if (!yamlContent)
		{
			CANDY_CORE_ERROR("Failed to read project file from VFS: {0}", vfsProjectPath);
			return;
		}

		auto project = Project::LoadFromVfs(*yamlContent, vfsProjectPath);
		if (project)
		{
			m_ActiveProject = project;
			UpdateWindowTitle();
			// Content is mounted separately (Engine/Game domains)
		}

		// Load ProjectSettings from VFS (Config/ProjectSetting.candy inside the Game domain)
		auto configYaml = FileSystem::Get().ReadText("VFS://Game/Config/ProjectSetting.candy");
		if (configYaml)
			ProjectSettings::Get().LoadFromString(*configYaml);
	}

	void Application::CreateProject(const std::filesystem::path& directory, const std::string& name)
	{
		auto project = Project::New(directory, name);
		if (project)
		{
			m_ActiveProject = project;
			UpdateWindowTitle();
		}
	}

	void Application::CloseProject()
	{
		m_ActiveProject.reset();
		UpdateWindowTitle();
	}

	void Application::UpdateWindowTitle()
	{
		if (m_ActiveProject)
		{
			std::string title = m_ActiveProject->GetName() + " - Candy Engine";
			m_Window->SetTitle(title);
		}
		else
		{
			m_Window->SetTitle("Candy Engine");
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}

	void Application::SchedulePopLayer(Layer* layer)
	{
		m_PendingPopLayer = layer;
	}

	void Application::SchedulePushLayer(Layer* layer)
	{
		m_PendingPushLayer = layer;
	}

	void Application::ProcessScheduledLayerChanges()
	{
		if (m_PendingPopLayer)
		{
			m_LayerStack.PopLayer(m_PendingPopLayer);
			m_PendingPopLayer = nullptr;
		}
		if (m_PendingPushLayer)
		{
			m_LayerStack.PushLayer(m_PendingPushLayer);
			m_PendingPushLayer = nullptr;
		}
	}

}
