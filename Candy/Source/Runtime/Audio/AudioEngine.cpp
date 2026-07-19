#include "CandyPCH.h"
#include "Runtime/Audio/AudioEngine.h"
#include "Runtime/Audio/Sound.h"
#include "Runtime/Core/Log.h"

#include "miniaudio.h"

namespace Candy {

	ma_engine* AudioEngine::s_Engine = nullptr;

	void AudioEngine::Init()
	{
		CANDY_CORE_ASSERT(!s_Engine, "AudioEngine already initialized!");

		s_Engine = new ma_engine();
		ma_result result = ma_engine_init(nullptr, s_Engine);
		if (result != MA_SUCCESS)
		{
			CANDY_CORE_ERROR("AudioEngine failed to initialize! error={0}", (int)result);
			delete s_Engine;
			s_Engine = nullptr;
			return;
		}

		CANDY_CORE_INFO("AudioEngine initialized successfully");
	}

	void AudioEngine::Shutdown()
	{
		if (s_Engine)
		{
			ma_engine_uninit(s_Engine);
			delete s_Engine;
			s_Engine = nullptr;
			CANDY_CORE_INFO("AudioEngine shutdown");
		}
	}

	void AudioEngine::PlayOneShot(const std::string& filepath, float volume)
	{
		if (!s_Engine)
		{
			CANDY_CORE_WARN("AudioEngine not initialized, cannot play: {0}", filepath);
			return;
		}

		ma_result result = ma_engine_play_sound(s_Engine, filepath.c_str(), nullptr);
		if (result != MA_SUCCESS)
			CANDY_CORE_ERROR("Failed to play one-shot sound: {0}", filepath);
	}

	Ref<Sound> AudioEngine::LoadSound(const std::string& filepath)
	{
		if (!s_Engine)
		{
			CANDY_CORE_WARN("AudioEngine not initialized, cannot load: {0}", filepath);
			return nullptr;
		}
		return CreateRef<Sound>(filepath);
	}

}
