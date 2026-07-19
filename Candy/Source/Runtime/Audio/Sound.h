#pragma once

#include "Runtime/Core/Base.h"

#include <string>

struct ma_sound;

namespace Candy {

	class Sound
	{
	public:
		Sound(const std::string& filepath);
		~Sound();

		void Play();
		void Stop();
		void Pause();

		void SetVolume(float volume);
		float GetVolume() const;

		void SetLooping(bool looping);
		bool IsLooping() const;

		bool IsPlaying() const;
		bool IsFinished() const;

		ma_sound* GetHandle() const { return m_Sound; }

	private:
		ma_sound* m_Sound = nullptr;

		friend class AudioEngine;
	};

}
