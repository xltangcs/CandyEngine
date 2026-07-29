#include "CandyPCH.h"

#include "Runtime/Renderer/GraphicsContext.h"

#include "Runtime/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLContext.h"

namespace Candy {

	Scope<GraphicsContext> GraphicsContext::Create(const WindowHandle& handle)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:    CANDY_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
		case RendererAPI::API::OpenGL:  return CreateScope<OpenGLContext>(handle);
		}

		CANDY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}