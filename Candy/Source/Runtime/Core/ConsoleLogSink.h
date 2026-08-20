#pragma once

#include <spdlog/sinks/base_sink.h>

#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace Candy {

	// Thread-safe ring buffer spdlog sink: collects log messages for in-engine
	// UI (e.g. the editor Console panel) without requiring a console window.
	class ConsoleLogSink : public spdlog::sinks::base_sink<spdlog::details::null_mutex>
	{
	public:
		struct Entry
		{
			spdlog::level::level_enum Level;
			std::chrono::system_clock::time_point Time;
			std::string LoggerName;
			std::string Message;
		};

		ConsoleLogSink() = default;

		/// Snapshot of all buffered entries (thread-safe copy).
		std::vector<Entry> GetEntries() const;

		void Clear();

		void SetMaxEntries(size_t maxEntries) { std::lock_guard<std::mutex> lock(m_Mutex); m_MaxEntries = maxEntries; }
		size_t GetMaxEntries() const { std::lock_guard<std::mutex> lock(m_Mutex); return m_MaxEntries; }

	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override;
		void flush_() override {}

	private:
		std::deque<Entry> m_Entries;
		size_t m_MaxEntries = 2000;
		mutable std::mutex m_Mutex;
	};

}
