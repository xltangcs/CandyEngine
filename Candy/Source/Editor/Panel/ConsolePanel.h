#pragma once

namespace Candy {

	// In-engine log viewer backed by Log::GetConsoleSink() (spdlog ring buffer).
	class ConsolePanel
	{
	public:
		ConsolePanel() = default;

		/// open: in/out visibility flag (window close button writes false).
		void OnImGuiRender(bool* open);
	};
}
