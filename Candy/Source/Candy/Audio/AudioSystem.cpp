#include "CandyPCH.h"
#include "Candy/Audio/AudioSystem.h"
#include "Candy/Audio/AudioEngine.h"
#include "Candy/Audio/Sound.h"
#include "Candy/Scene/Scene.h"
#include "Candy/Scene/Entity.h"
#include "Candy/Scene/Components.h"

#include "miniaudio.h"

namespace Candy {

	void AudioSystem::OnRuntimeStart(Scene& scene)
	{
		auto view = scene.GetAllEntitiesWith<AudioSourceComponent>();
		for (auto entityHandle : view)
		{
			Entity entity = { entityHandle, &scene };
			auto& asc = entity.GetComponent<AudioSourceComponent>();

			if (!asc.SoundPath.empty() && asc.PlayOnStart)
			{
				ma_engine* engine = AudioEngine::GetEngineHandle();
				if (engine)
				{
					ma_sound* sound = new ma_sound();
					ma_result result = ma_sound_init_from_file(engine, asc.SoundPath.c_str(),
						MA_SOUND_FLAG_ASYNC, nullptr, nullptr, sound);
					if (result == MA_SUCCESS)
					{
						ma_sound_set_volume(sound, asc.Volume);
						ma_sound_set_looping(sound, asc.Looping);
						ma_sound_start(sound);
						asc.RuntimeHandle = sound;
					}
					else
					{
						delete sound;
						CANDY_CORE_ERROR("AudioSystem: failed to load {0}", asc.SoundPath);
					}
				}
			}
		}
	}

	void AudioSystem::OnRuntimeStop(Scene& scene)
	{
		auto view = scene.GetAllEntitiesWith<AudioSourceComponent>();
		for (auto entityHandle : view)
		{
			Entity entity = { entityHandle, &scene };
			auto& asc = entity.GetComponent<AudioSourceComponent>();

			if (asc.RuntimeHandle)
			{
				ma_sound* sound = (ma_sound*)asc.RuntimeHandle;
				ma_sound_stop(sound);
				ma_sound_uninit(sound);
				delete sound;
				asc.RuntimeHandle = nullptr;
			}
		}
	}

	void AudioSystem::OnUpdateRuntime(Scene& scene, Timestep ts)
	{
		// Future: 3D audio position sync from TransformComponent
		// For now, 2D-only playback, no per-frame updates needed
	}

}
