#pragma once

#include "Candy/Core/Base.h"
#include "Candy/Core/Window.h"
#include "Candy/Core/Timestep.h"
#include "Candy/Core/LayerStack.h"

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

		inline Window& GetWindow() { return *m_Window; }
		void Close();

		ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }
		inline static Application& Get() { return *s_Instance; }

	private:
		void Run();
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);
	private:
		bool m_Running = true;
		bool m_Minimized = false;
		float m_LastFrameTime = 0.0f;

		std::unique_ptr<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		LayerStack m_LayerStack;
		static Application* s_Instance;

		friend int ::main(int argc, char** argv);
	};


	Application* CreateApplication();
}

