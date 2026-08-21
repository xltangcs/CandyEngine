#include "CandyPCH.h"

#include "Platform/OpenGL/OpenGLContext.h"

#include "Platform/OpenGL/OpenGLRHIDevice.h"
#include "Runtime/RHI/RHIContext.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

// OpenGL backend system dependency (WGL). Propagates through Candy.lib to any
// final executable via /DEFAULTLIB — see D3D12Device.cpp for the same pattern.
#pragma comment(lib, "opengl32.lib")

namespace Candy {

	#ifdef CANDY_DEBUG
	static void OpenGLMessageCallback(
		unsigned source,
		unsigned type,
		unsigned id,
		unsigned severity,
		int length,
		const char* message,
		const void* userParam)
	{
		switch (severity)
		{
		case GL_DEBUG_SEVERITY_HIGH:         CANDY_CORE_CRITICAL(message); return;
		case GL_DEBUG_SEVERITY_MEDIUM:       CANDY_CORE_ERROR(message); return;
		case GL_DEBUG_SEVERITY_LOW:          CANDY_CORE_WARN(message); return;
		case GL_DEBUG_SEVERITY_NOTIFICATION: CANDY_CORE_TRACE(message); return;
		}

		CANDY_CORE_ASSERT(false, "Unknown severity level!");
	}
	#endif

	OpenGLContext::OpenGLContext(const WindowHandle& handle)
		: m_WindowHandle(static_cast<GLFWwindow*>(handle.Native))
	{
		CANDY_CORE_ASSERT(m_WindowHandle, "Window handle is null!")
	}

	void OpenGLContext::Init()
	{
		glfwMakeContextCurrent(m_WindowHandle);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		CANDY_CORE_ASSERT(status, "Failed to initialize Glad!");

		CANDY_CORE_ASSERT(GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 5),
		                 "CandyEngine requires at least OpenGL version 4.5!");

	#ifdef CANDY_DEBUG
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(OpenGLMessageCallback, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
	#endif

		// RHI adapter layer: OpenGLRHIDevice owns the IR subsystems
		// (Pipeline cache / shader library / etc.) and registers its own
		// OpenGLRHICommandQueue.  All Runtime code reaches the device via
		// RHIContext::GetDevice() so it never has to include Platform headers.
		m_RHIDevice = std::make_unique<OpenGLRHIDevice>();

		int w = 1280, h = 720;
		glfwGetFramebufferSize(m_WindowHandle, &w, &h);
		if (w <= 0 || h <= 0) { w = 1280; h = 720; }

		SwapChainDesc scDesc;
		scDesc.Width       = static_cast<uint32_t>(w);
		scDesc.Height      = static_cast<uint32_t>(h);
		scDesc.BufferCount = 2;
		scDesc.VSync       = true;
		m_RHISwapChain = m_RHIDevice->CreateSwapChain(scDesc);

		RHIContext::SetDevice(m_RHIDevice.get());
		RHIContext::SetSwapChain(m_RHISwapChain.get());

		CANDY_CORE_INFO("OpenGLContext: initialized ({}x{}, RHI device ready)", w, h);
	}

	void OpenGLContext::SwapBuffers()
	{
		glfwSwapBuffers(m_WindowHandle);
	}

}
