#include "CandyPCH.h"

#include "Platform/OpenGL/OpenGLContext.h"

#include "Platform/OpenGL/OpenGLRHIDevice.h"
#include "Runtime/RHI/RHIContext.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

namespace Candy {

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
