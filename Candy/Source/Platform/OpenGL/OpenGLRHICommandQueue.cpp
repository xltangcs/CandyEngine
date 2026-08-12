#include "CandyPCH.h"

#include "Platform/OpenGL/OpenGLRHICommandQueue.h"
#include "Platform/OpenGL/OpenGLRHICommandBuffer.h"

#include <glad/glad.h>
namespace Candy {

	Scope<RHICommandBuffer> OpenGLRHICommandQueue::CreateCommandBuffer()
	{
		return CreateScope<OpenGLRHICommandBuffer>(m_DefaultVAO);
	}

	void OpenGLRHICommandQueue::Submit(const std::vector<RHICommandBuffer*>& cmds)
	{
		(void)cmds;
		// OpenGL is immediate-mode; all GL calls were issued during recording.
		// Flush marks end-of-frame submission; present is handled separately.
		glFlush();
	}

	void OpenGLRHICommandQueue::Present(const Ref<RHISwapChain>& swapChain)
	{
		(void)swapChain;
		// GL default framebuffer presentation is handled by
		// GraphicsContext::SwapBuffers (GLFW).  No-op here.
	}

	void OpenGLRHICommandQueue::WaitIdle()
	{
		glFinish();
	}

} // namespace Candy
