#pragma once

#include "Core.h"
#include "Window.h"

#include "Events/Event.h"
#include "Candy/LayerStack.h"
#include "Candy/Events/ApplicationEvent.h"
#include "Candy/Imgui/ImguiLayer.h"

#include "Candy/Renderer/Shader.h"
#include "Candy/Renderer/Buffer.h"
#include "Candy/Renderer/VertexArray.h"

namespace Candy {

	class CANDY_API Application
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
		std::unique_ptr<Window> m_Window;

		ImGuiLayer* m_ImGuiLayer;

		bool m_Running = true;
		LayerStack m_LayerStack;

		std::shared_ptr<Shader> m_Shader;
		std::shared_ptr<VertexArray> m_VertexArray;

		std::shared_ptr<Shader> m_BlueShader;
		std::shared_ptr<VertexArray> m_SquareVA;

	private:
		static Application* s_Instance;

	};


	Application* CreateApplication();
}

