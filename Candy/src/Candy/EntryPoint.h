#pragma once

#ifdef CANDY_PLATFORM_WINDOWS

extern Candy::Application* Candy::CreateApplication();

int main(int argc, char** argv)
{
	Candy::Log::Init();
	//Candy::Log::GetCoreLogger()->warn("Initialized Log!");
	//Candy::Log::GetClientLogger()->info("Hello Candy!");

	CANDY_CORE_WARN("Initialized Log!");
	int a = 5;
	CANDY_CORE_INFO("Hello! Var = {0}", a);


	auto app = Candy::CreateApplication();
	app->Run();
	delete app;
}

#endif