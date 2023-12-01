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


#ifdef CANDY_ENABLE_ASSERTS
    #define CANDY_ASSERT(x, ...) { if(!(x)) { CANDY_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
    #define CANDY_CORE_ASSERT(x, ...) { if(!(x)) { CANDY_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
    #define CANDY_ASSERT(x, ...)
    #define CANDY_CORE_ASSERT(x, ...)
#endif


#define BIT(x) (1 << x)
