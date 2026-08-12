#pragma once

#include "Runtime/RHI/RHITypes.h"

namespace Candy {

	class GraphicsContext
	{
	public:
		virtual ~GraphicsContext() = default;
		virtual void Init() = 0;
		virtual void SwapBuffers() = 0;

		/// Called when the host window is resized so backends can resize their
		/// swap chain / back buffers to match. Default is a no-op (OpenGL's
		/// default framebuffer follows the window automatically).
		virtual void OnResize(uint32_t width, uint32_t height) {}

		static Scope<GraphicsContext> Create(const WindowHandle& handle);
	};

}