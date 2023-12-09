#include "candypch.h"

#include "Platform/OpenGL/OpenGLContext.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <GL/GL.h>

namespace Candy {

	OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		CANDY_CORE_ASSERT(windowHandle, "Window handle is null!")
	}

	void OpenGLContext::Init()
	{
		glfwMakeContextCurrent(m_WindowHandle);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		CANDY_CORE_ASSERT(status, "Failed to initialize Glad!");

		//CANDY_CORE_INFO("  OpenGL Info:");
		//CANDY_CORE_INFO("  Vendor: {0}", glGetString(GL_VENDOR));
		//CANDY_CORE_INFO("  Renderer: {0}", glGetString(GL_RENDERER));
		//CANDY_CORE_INFO("  Version: {0}", glGetString(GL_VERSION));
	}

	void OpenGLContext::SwapBuffers()
	{
		glfwSwapBuffers(m_WindowHandle);
	}

}

