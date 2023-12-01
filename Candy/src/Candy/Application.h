#pragma once

#include "Core.h"

namespace Candy {

	class CANDY_API Application
	{
	public:
		Application();
		virtual ~Application();
		void Run();
	};

	Application* CreateApplication();
}

