#pragma once
#include "Candy/Core/Base.h"

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"


namespace Candy {
	class Log {
	public:
		static void Init();

		inline static Ref<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static Ref<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
	private:
		static Ref<spdlog::logger> s_CoreLogger;
		static Ref<spdlog::logger> s_ClientLogger;
	};
}

// Core log macros
#define CANDY_CORE_TRACE(...)       ::Candy::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define CANDY_CORE_INFO(...)        ::Candy::Log::GetCoreLogger()->info(__VA_ARGS__)
#define CANDY_CORE_WARN(...)        ::Candy::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define CANDY_CORE_ERROR(...)       ::Candy::Log::GetCoreLogger()->error(__VA_ARGS__)
#define CANDY_CORE_CRITICAL(...)    ::Candy::Log::GetCoreLogger()->critical(__VA_ARGS__) 
									  
// Client log macros				  
#define CANDY_TRACE(...)	        ::Candy::Log::GetClientLogger()->trace(__VA_ARGS__)
#define CANDY_INFO(...)	            ::Candy::Log::GetClientLogger()->info(__VA_ARGS__)
#define CANDY_WARN(...)	            ::Candy::Log::GetClientLogger()->warn(__VA_ARGS__)
#define CANDY_ERROR(...)	        ::Candy::Log::GetClientLogger()->error(__VA_ARGS__)
#define CANDY_CRITICAL(...)	        ::Candy::Log::GetClientLogger()->critical(__VA_ARGS__)
