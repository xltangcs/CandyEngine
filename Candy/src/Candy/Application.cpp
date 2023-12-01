#include "Application.h"
#include "Candy/Events/ApplicationEvent.h"
#include "Candy/Log.h"


namespace Candy {
	Application::Application()
	{

	}

	Application::~Application()
	{

	}

	void Application::Run()
	{
		WindowResizeEvent e(1280, 720);
		if (e.IsInCategory(EventCategoryApplication))
		{
			CANDY_TRACE(e);
		}
		if (e.IsInCategory(EventCategoryInput))
		{
			CANDY_TRACE(e);
		}
		while (true)
		{
			//std::cout<<"Hello, Candy Engine!\n";
		}
	}
}