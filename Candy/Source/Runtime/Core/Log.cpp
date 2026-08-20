#include "CandyPCH.h"

#include "Runtime/Core/Log.h"
#include "Runtime/Core/ConsoleLogSink.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <filesystem>

namespace Candy {

	Ref<spdlog::logger> Log::s_CoreLogger;
	Ref<spdlog::logger> Log::s_ClientLogger;
	Ref<ConsoleLogSink> Log::s_ConsoleSink;

	void Log::Init()
	{
		std::filesystem::create_directories("Saved");

		std::vector<spdlog::sink_ptr> logSinks;

		// Console sink only when a real console is attached (CandyGame / PakTool);
		// the editor is a WindowedApp without one and relies on the Console panel.
#ifdef CANDY_PLATFORM_WINDOWS
		HANDLE stdoutHandle = ::GetStdHandle(STD_OUTPUT_HANDLE);
		if (stdoutHandle != nullptr && stdoutHandle != INVALID_HANDLE_VALUE)
		{
			auto consoleSink = CreateRef<spdlog::sinks::stdout_color_sink_mt>();
			consoleSink->set_pattern("%^[%T] %n: %v%$");
			logSinks.push_back(consoleSink);
		}
#else
		{
			auto consoleSink = CreateRef<spdlog::sinks::stdout_color_sink_mt>();
			consoleSink->set_pattern("%^[%T] %n: %v%$");
			logSinks.push_back(consoleSink);
		}
#endif
		{
			auto fileSink = CreateRef<spdlog::sinks::basic_file_sink_mt>("Saved/CandyEngine.log", true);
			fileSink->set_pattern("[%T] [%l] %n: %v");
			logSinks.push_back(fileSink);
		}

		s_ConsoleSink = CreateRef<ConsoleLogSink>();
		logSinks.push_back(s_ConsoleSink);

		s_CoreLogger = CreateRef<spdlog::logger>("CANDY", begin(logSinks), end(logSinks));
		spdlog::register_logger(s_CoreLogger);
		s_CoreLogger->set_level(spdlog::level::trace);
		s_CoreLogger->flush_on(spdlog::level::trace);

		s_ClientLogger = CreateRef<spdlog::logger>("APP", begin(logSinks), end(logSinks));
		spdlog::register_logger(s_ClientLogger);
		s_ClientLogger->set_level(spdlog::level::trace);
		s_ClientLogger->flush_on(spdlog::level::trace);
	}
}
