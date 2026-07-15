#pragma once

#include "Candy/Core/Base.h"
#include "Candy/Core/Window.h"
#include "Candy/Core/Timestep.h"
#include "Candy/Core/LayerStack.h"
#include "Candy/Audio/AudioEngine.h"
#include "Candy/Project/Project.h"

#include "Candy/Events/Event.h"
#include "Candy/Events/ApplicationEvent.h"

#include "Candy/Imgui/ImguiLayer.h"

int main(int argc, char** argv);

namespace Candy {

	class Application
	{
	public:
		Application(const std::string& name = "Candy Engine");
		virtual ~Application();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);
		void PopLayer(Layer* layer);
		void PopOverlay(Layer* layer);

		inline Window& GetWindow() { return *m_Window; }
		void Close();

		ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }
		inline static Application& Get() { return *s_Instance; }

		// Project management
		Ref<Project> GetProject() const { return m_ActiveProject; }
		void LoadProject(const std::filesystem::path& projectFile);
		void CreateProject(const std::filesystem::path& directory, const std::string& name);
		void CloseProject();
		bool IsProjectLoaded() const { return m_ActiveProject != nullptr; }

		void UpdateWindowTitle();

		// Deferred layer transitions (safe to call from within layer callbacks)
		void SchedulePopLayer(Layer* layer);
		void SchedulePushLayer(Layer* layer);
		void ProcessScheduledLayerChanges();

	private:
		void Run();
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);
	private:
		bool m_Running = true;
		bool m_Minimized = false;
		float m_LastFrameTime = 0.0f;

		Scope<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		
		Ref<Project> m_ActiveProject;

		LayerStack m_LayerStack;
		
		Layer* m_PendingPopLayer = nullptr;
		Layer* m_PendingPushLayer = nullptr;

		static Application* s_Instance;

		friend int ::main(int argc, char** argv);
	};


	Application* CreateApplication(int argc, char** argv);
}

