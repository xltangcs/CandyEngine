#include "CandyPCH.h"

#include "Runtime/Core/ConsoleLogSink.h"

namespace Candy {

	void ConsoleLogSink::sink_it_(const spdlog::details::log_msg& msg)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);

		m_Entries.push_back(Entry{
			msg.level, msg.time,
			std::string(msg.logger_name.data(), msg.logger_name.size()),
			std::string(msg.payload.data(), msg.payload.size()) });
		while (m_Entries.size() > m_MaxEntries)
			m_Entries.pop_front();
	}

	std::vector<ConsoleLogSink::Entry> ConsoleLogSink::GetEntries() const
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		return { m_Entries.begin(), m_Entries.end() };
	}

	void ConsoleLogSink::Clear()
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Entries.clear();
	}

}
