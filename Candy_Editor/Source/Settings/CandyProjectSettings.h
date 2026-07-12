#pragma once

namespace Candy {

	class CandyProjectSettings
	{
	public:
		static CandyProjectSettings& Get();

		void OnImGuiRender();
		void Save();
		void Load();

	private:
		CandyProjectSettings() = default;
	};

}
