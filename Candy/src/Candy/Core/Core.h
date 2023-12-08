#pragma once

#include <memory>

#ifdef  CANDY_PLATFORM_WINDOWS
	#if CANDY_DYNAMIC_LINK
		#ifdef CANDY_BUILD_DLL
			#define CANDY_API _declspec(dllexport)
		#else
			#define CANDY_API _declspec(dllimport)
		#endif // CANDY_BUILD_DLL
	#else
		#define CANDY_API
	#endif
#else
	#error CandyEngine only support windows
#endif //  CANDY_PLATFORM_WINDOWS

#ifdef CANDY_DEBUG
	#define  CANDY_ENABLE_ASSERTS
#endif


#ifdef CANDY_ENABLE_ASSERTS
    #define CANDY_ASSERT(x, ...) { if(!(x)) { CANDY_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
    #define CANDY_CORE_ASSERT(x, ...) { if(!(x)) { CANDY_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
    #define CANDY_ASSERT(x, ...)
    #define CANDY_CORE_ASSERT(x, ...)
#endif


#define BIT(x) (1 << x)

#define CANDY_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)

namespace Candy {
	template<typename T>
	using Scope = std::unique_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Scope<T> CreateScope(Args&& ... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}
	template<typename T>
	using Ref = std::shared_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Ref<T> CreateRef(Args&& ... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}
}