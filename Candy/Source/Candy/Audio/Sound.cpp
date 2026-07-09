#include "CandyPCH.h"
#include "Candy/Audio/Sound.h"
#include "Candy/Audio/AudioEngine.h"
#include "Candy/Core/Log.h"

#include "miniaudio.h"

namespace Candy {

	Sound::Sound(const std::string& filepath)
	{
		ma_engine* engine = AudioEngine::GetEngineHandle();
		if (!engine)
		{
			CANDY_CORE_WARN("Sound: AudioEngine not available");
			return;
		}

		m_Sound = new ma_sound();
		ma_result result = ma_sound_init_from_file(engine, filepath.c_str(),
			MA_SOUND_FLAG_ASYNC, nullptr, nullptr, m_Sound);
		if (result != MA_SUCCESS)
		{
			CANDY_CORE_ERROR("Failed to load sound: {0}", filepath);
			delete m_Sound;
			m_Sound = nullptr;
		}
	}

	Sound::~Sound()
	{
		if (m_Sound)
		{
			ma_sound_uninit(m_Sound);
			delete m_Sound;
			m_Sound = nullptr;
		}
	}

	void Sound::Play()
	{
		if (m_Sound)
			ma_sound_start(m_Sound);
	}

	void Sound::Stop()
	{
		if (m_Sound)
			ma_sound_stop(m_Sound);
	}

	void Sound::Pause()
	{
		if (m_Sound)
			ma_sound_stop(m_Sound);
	}

	void Sound::SetVolume(float volume)
	{
		if (m_Sound)
			ma_sound_set_volume(m_Sound, volume);
	}

	float Sound::GetVolume() const
	{
		if (m_Sound)
			return ma_sound_get_volume(m_Sound);
		return 0.0f;
	}

	void Sound::SetLooping(bool looping)
	{
		if (m_Sound)
			ma_sound_set_looping(m_Sound, looping);
	}

	bool Sound::IsLooping() const
	{
		if (m_Sound)
			return ma_sound_is_looping(m_Sound);
		return false;
	}

	bool Sound::IsPlaying() const
	{
		if (m_Sound)
			return ma_sound_is_playing(m_Sound);
		return false;
	}

	bool Sound::IsFinished() const
	{
		if (m_Sound)
			return !ma_sound_is_playing(m_Sound);
		return true;
	}

}
