#pragma once

#include "Candy/Core/Base.h"
#include <filesystem>
#ifdef CANDY_PLATFORM_WINDOWS

extern Candy::Application* Candy::CreateApplication();

int main(int argc, char** argv)
{
	Candy::Log::Init();

	CANDY_CORE_INFO("Initialized Log!");
	
	CANDY_PROFILE_BEGIN_SESSION("Startup", "Saved/CandyProfile-Startup.json");
	auto app = Candy::CreateApplication();
	CANDY_PROFILE_END_SESSION();

	CANDY_PROFILE_BEGIN_SESSION("Runtime", "Saved/CandyProfile-Runtime.json");
	app->Run();
	CANDY_PROFILE_END_SESSION();

	CANDY_PROFILE_BEGIN_SESSION("Shutdown", "Saved/CandyProfile-Shutdown.json");
	delete app;
	CANDY_PROFILE_END_SESSION();
}

#endif