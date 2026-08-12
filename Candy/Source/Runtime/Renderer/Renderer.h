#pragma once

#include "Runtime/Renderer/RendererAPI.h"

namespace Candy {

	class Renderer
	{
	public:
		static void Init();
		static void Shutdown();

		inline static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }
	};
}
