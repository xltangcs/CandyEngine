#pragma once

#include "Runtime/Core/Base.h"

#include <string>

struct ma_engine;

namespace Candy {

	class Sound;

	class AudioEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static void PlayOneShot(const std::string& filepath, float volume = 1.0f);
		static Ref<Sound> LoadSound(const std::string& filepath);

		static ma_engine* GetEngineHandle() { return s_Engine; }

	private:
		static ma_engine* s_Engine;
	};

}
