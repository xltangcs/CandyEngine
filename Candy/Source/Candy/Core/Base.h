#pragma once

#include <memory>

// Platform detection using predefined macros
#ifdef _WIN32
		/* Windows x64/x86 */
	#ifdef _WIN64
		/* Windows x64  */
	#define CANDY_PLATFORM_WINDOWS
	#else
		/* Windows x86 */
	#error "x86 Builds are not supported!"
	#endif
#else
	#error CandyEngine only support windows
#endif


#ifdef CANDY_DEBUG
	#if defined(CANDY_PLATFORM_WINDOWS)
		#define CANDY_DEBUGBREAK() __debugbreak()
	#elif defined(CANDY_PLATFORM_LINUX)
		#include <signal.h>
		#define CANDY_DEBUGBREAK() raise(SIGTRAP)
	#else
		#error "Platform doesn't support debugbreak yet!"
	#endif
	#define CANDY_ENABLE_ASSERTS
#else
	#define CANDY_DEBUGBREAK()
#endif


#define CANDY_EXPAND_MACRO(x) x
#define CANDY_STRINGIFY_MACRO(x) #x


#define BIT(x) (1 << x)

//#define CANDY_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)
#define CANDY_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

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


#include "Candy/Core/Log.h"
#include "Candy/Core/Assert.h"
#include "Candy/Debug/Instrumentor.h"