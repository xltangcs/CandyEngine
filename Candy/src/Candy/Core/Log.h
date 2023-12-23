#pragma once
#include "Candy/Core/Base.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/string_cast.hpp"

// This ignores all warnings raised inside External headers
#pragma warning(push, 0)
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#pragma warning(pop)


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

template<typename OStream, glm::length_t L, typename T, glm::qualifier Q>
inline OStream& operator<<(OStream& os, const glm::vec<L, T, Q>& vector)
{
	return os << glm::to_string(vector);
}

template<typename OStream, glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
inline OStream& operator<<(OStream& os, const glm::mat<C, R, T, Q>& matrix)
{
	return os << glm::to_string(matrix);
}

template<typename OStream, typename T, glm::qualifier Q>
inline OStream& operator<<(OStream& os, glm::qua<T, Q> quaternio)
{
	return os << glm::to_string(quaternio);
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
