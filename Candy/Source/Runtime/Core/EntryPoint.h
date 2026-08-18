#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/Core/FileSystem.h"
#include <filesystem>
#ifdef CANDY_PLATFORM_WINDOWS

extern Candy::Application* Candy::CreateApplication(int argc, char** argv);

int main(int argc, char** argv)
{
	Candy::Log::Init();

	CANDY_CORE_INFO("Initialized Log!");

	// Mount VFS://Engine before CreateApplication() — EditorSettings/EditorState
	// load from VFS://Engine/Config + VFS://Engine/Saved during app construction,
	// so user data (imgui.ini, EditorState.candy, EditorSettings.candy, layout
	// presets) resolves regardless of the process cwd.
	Candy::FileSystem::Get().BootstrapMount();
	
	CANDY_PROFILE_BEGIN_SESSION("Startup", "Saved/CandyProfile-Startup.json");
	auto app = Candy::CreateApplication(argc, argv);
	CANDY_PROFILE_END_SESSION();

	CANDY_PROFILE_BEGIN_SESSION("Runtime", "Saved/CandyProfile-Runtime.json");
	app->Run();
	CANDY_PROFILE_END_SESSION();

	CANDY_PROFILE_BEGIN_SESSION("Shutdown", "Saved/CandyProfile-Shutdown.json");
	delete app;
	CANDY_PROFILE_END_SESSION();
}

#endif