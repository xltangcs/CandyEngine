#include "candypch.h"
#include "RendererCommand.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace Candy {
	RendererAPI* RenderCommand::s_RendererAPI = new OpenGLRendererAPI;
}