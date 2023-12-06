#pragma once

#include "Core.h"
#include "Window.h"

#include "Events/Event.h"
#include "Candy/LayerStack.h"
#include "Candy/Events/ApplicationEvent.h"
#include "Candy/Imgui/ImguiLayer.h"

#include "Candy/Core/Timestep.h"

namespace Candy {

	class Application
	{
	public:
		Application();
		virtual ~Application();
		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		inline Window& GetWindow() { return *m_Window; }

		inline static Application& Get() { return *s_Instance; }

	private:
		bool OnWindowClose(WindowCloseEvent& e);

	private:
		std::unique_ptr<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		bool m_Running = true;
		LayerStack m_LayerStack;
		float m_LastFrameTime = 0.0f;
		static Application* s_Instance;

	};


	Application* CreateApplication();
}

