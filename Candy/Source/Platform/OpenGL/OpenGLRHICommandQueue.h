#pragma once

#include "Runtime/RHI/RHICommandQueue.h"

#include <glad/glad.h>

namespace Candy {

	class OpenGLRHICommandBuffer;

	// =========================================================================
	// OpenGLRHICommandQueue — bookkeeping for per-frame command buffers.
	// OpenGL is immediate-mode; "submit" is just glFlush (SwapBuffers handled
	// by GraphicsContext).
	// =========================================================================
	class OpenGLRHICommandQueue : public RHICommandQueue
	{
	public:
		explicit OpenGLRHICommandQueue(GLuint defaultVAO) : m_DefaultVAO(defaultVAO) {}
		~OpenGLRHICommandQueue() override = default;

		// RHICommandQueue overrides — see RHICommandQueue.h
		Scope<RHICommandBuffer> CreateCommandBuffer() override;
		void Submit(const std::vector<RHICommandBuffer*>& cmds) override;
		void Present(const Ref<RHISwapChain>& swapChain) override;
		void WaitIdle() override;

	private:
		GLuint m_DefaultVAO = 0;
	};

} // namespace Candy
