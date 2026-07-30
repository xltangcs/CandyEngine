#pragma once

#include "Runtime/Core/Window.h"
#include "Runtime/Renderer/GraphicsContext.h"

#include <GLFW/glfw3.h>

namespace Candy {

	class WindowsWindow : public Window
	{
	public:
		WindowsWindow(const WindowProps& props);
		virtual ~WindowsWindow();

		void OnUpdate() override;

		inline unsigned int GetWidth() const override { return m_Data.Width; }
		inline unsigned int GetHeight() const override { return m_Data.Height; }

		// Window attributes
		inline void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }
		void SetVSync(bool enabled) override;
		bool IsVSync() const override;

		void SetTitle(const std::string& title) override { glfwSetWindowTitle(m_Window, title.c_str()); }
		void SetSize(uint32_t w, uint32_t h) override;

		inline virtual void* GetNativeWindow() const { return m_Window; }

		void* GetNativeWindowHandle() const override;

		/// Returns the graphics context for backend-specific access (D3D12/Vulkan).
		GraphicsContext* GetGraphicsContext() override { return m_Context.get(); }

	private:
		virtual void Init(const WindowProps& props);
		virtual void Shutdown();
	private:
		GLFWwindow* m_Window;
		Scope<GraphicsContext> m_Context;

		struct WindowData
		{
			std::string Title;
			unsigned int Width, Height;
			bool VSync;

			EventCallbackFn EventCallback;
		};

		WindowData m_Data;
	};

}