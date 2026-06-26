#include "candypch.h"

#include "Candy/Renderer/RenderCommand.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace Candy {
	Scope<RendererAPI> RenderCommand::s_RendererAPI = RendererAPI::Create();
}