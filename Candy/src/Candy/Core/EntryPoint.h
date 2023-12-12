#pragma once

#include "Candy/Core/Base.h"

#ifdef CANDY_PLATFORM_WINDOWS

extern Candy::Application* Candy::CreateApplication();

int main(int argc, char** argv)
{
	Candy::Log::Init();

	CANDY_CORE_INFO("Initialized Log!");
	
	auto app = Candy::CreateApplication();
	app->Run();
	delete app;
}

#endif