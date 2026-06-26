#include "candypch.h"
#include "Candy/Core/Window.h"

#ifdef CANDY_PLATFORM_WINDOWS
	#include "Platform/Windows/WindowsWindow.h"
#endif

namespace Candy
{
	Scope<Window> Window::Create(const WindowProps& props)
	{
#ifdef CANDY_PLATFORM_WINDOWS
		return CreateScope<WindowsWindow>(props);
#else
		CANDY_CORE_ASSERT(false, "Unknown platform!");
		return nullptr;
#endif
	}

}