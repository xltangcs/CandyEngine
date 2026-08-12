#include "CandyPCH.h"

#include "Runtime/Renderer/Renderer.h"
#include "Runtime/Renderer/Renderer2D.h"

namespace Candy {

	void Renderer::Init()
	{
		Renderer2D::Init();
	}

	void Renderer::Shutdown()
	{
		Renderer2D::Shutdown();
	}
}
