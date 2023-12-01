#pragma once


#ifdef  CANDY_PLATFORM_WINDOWS
	#ifdef CANDY_BUILD_DLL
		#define CANDY_API _declspec(dllexport)
	#else
		#define CANDY_API _declspec(dllimport)
	#endif // CANDY_BUILD_DLL
#else
	#error CandyEngine only support windows
#endif //  CANDY_PLATFORM_WINDOWS

#define BIT(x) (1 << x)
