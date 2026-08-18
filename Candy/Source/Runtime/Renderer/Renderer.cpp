#include "CandyPCH.h"

#include "Runtime/Renderer/Renderer.h"
#include "Runtime/Renderer/SceneRenderer.h"

namespace Candy {

	void Renderer::Init()
	{
		SceneRenderer::Init();
	}

	void Renderer::Shutdown()
	{
		SceneRenderer::Shutdown();
	}
}
