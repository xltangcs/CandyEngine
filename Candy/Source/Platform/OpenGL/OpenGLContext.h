#pragma once

#include "Runtime/Renderer/GraphicsContext.h"
#include "Runtime/Core/Base.h"

#include <memory>

struct GLFWwindow;

namespace Candy {

	class OpenGLRHIDevice;
	class RHISwapChain;

	class OpenGLContext : public GraphicsContext
	{
	public:
		OpenGLContext(const WindowHandle& handle);

		virtual void Init() override;
		virtual void SwapBuffers() override;

	private:
		GLFWwindow*                              m_WindowHandle;
		std::unique_ptr<OpenGLRHIDevice>         m_RHIDevice;
		Ref<RHISwapChain>                        m_RHISwapChain;
	};
}
