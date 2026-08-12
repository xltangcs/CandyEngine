#include "CandyPCH.h"

#include "Platform/OpenGL/OpenGLImGuiBackend.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

namespace Candy {

	void OpenGLImGuiBackend::Init(GLFWwindow* window)
	{
		ImGui_ImplGlfw_InitForOpenGL(window, true);
	}

	void OpenGLImGuiBackend::InitContext()
	{
		ImGui_ImplOpenGL3_Init("#version 410");
	}

	void OpenGLImGuiBackend::ShutdownContext()
	{
		ImGui_ImplOpenGL3_Shutdown();
	}

	void OpenGLImGuiBackend::Shutdown()
	{
		ImGui_ImplGlfw_Shutdown();
	}

	void OpenGLImGuiBackend::NewFrame()
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
	}

	void OpenGLImGuiBackend::NewFrameGameUI()
	{
		// Renderer only — the game UI overlays the viewport and must not advance
		// the GLFW platform frame.
		ImGui_ImplOpenGL3_NewFrame();
	}

	void OpenGLImGuiBackend::RenderDrawData(ImDrawData* drawData, Framebuffer* /*target*/)
	{
		// OpenGL renders into whatever framebuffer is currently bound by the
		// caller (viewport FBO for game UI, default framebuffer for the editor).
		ImGui_ImplOpenGL3_RenderDrawData(drawData);

		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow* backupCurrentContext = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backupCurrentContext);
		}
	}
}
