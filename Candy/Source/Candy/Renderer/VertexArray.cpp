#include "candypch.h"

#include "Candy/Renderer/Renderer.h"
#include "Candy/Renderer/VertexArray.h"

#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace Candy {
	Ref<VertexArray> VertexArray::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    CANDY_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLVertexArray>();
		}

		CANDY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}