#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/Core/Window.h"
#include "Runtime/Core/Timestep.h"
#include "Runtime/Core/LayerStack.h"
#include "Runtime/Audio/AudioEngine.h"
#include "Runtime/Project/Project.h"

#include "Runtime/Events/Event.h"
#include "Runtime/Events/ApplicationEvent.h"

#include "Runtime/Imgui/ImguiLayer.h"

int main(int argc, char** argv);

namespace Candy {

	class Application
	{
	public:
		Application(const std::string& name = "Candy Engine",
			uint32_t width = 1280,
			uint32_t height = 720,
			bool resizable = true,
			bool isEditor = true);
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
		void LoadProjectFromVfs(const std::string& vfsProjectPath);
		void CreateProject(const std::filesystem::path& directory, const std::string& name);
		void CloseProject();
		bool IsProjectLoaded() const { return m_ActiveProject != nullptr; }

		void UpdateWindowTitle();

		// Deferred layer transitions (safe to call from within layer callbacks)
		void SchedulePopLayer(Layer* layer);
		void SchedulePushLayer(Layer* layer);
		void ProcessScheduledLayerChanges();

		void SetFontSize(float size) { m_FontSize = size; }
		const float GetFontSize() const { return m_FontSize; }
		void SetFontPath(const std::string& path) { m_FontPath = path; }
		const std::string GetFontPath() const { return m_FontPath; }

	private:
		void Run();
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);
	private:
		bool m_Running = true;
		bool m_Minimized = false;
		float m_LastFrameTime = 0.0f;
		bool m_IsEditor = true;

		float m_FontSize = 18.0f;
		std::string m_FontPath = "/engine/Fonts/opensans/OpenSans-Regular.ttf";

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

