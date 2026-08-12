#pragma once

#include "Runtime/Imgui/ImGuiBackend.h"

namespace Candy {

	// OpenGL ImGui backend — ImGui_ImplOpenGL3 + GLFW platform (the only
	// backend that wires ImGui multi-viewport platform windows).
	class OpenGLImGuiBackend : public ImGuiBackend
	{
	public:
		void Init(GLFWwindow* window) override;
		void InitContext() override;
		void ShutdownContext() override;
		void Shutdown() override;

		bool SupportsPlatformWindows() const override { return true; }

		void NewFrame() override;
		void NewFrameGameUI() override;
		void RenderDrawData(ImDrawData* drawData, Framebuffer* target) override;
	};
}
