#pragma once

namespace Candy {

	class Scene;
	class Timestep;

	class AudioSystem
	{
	public:
		static void OnUpdateRuntime(Scene& scene, Timestep ts);
		static void OnRuntimeStart(Scene& scene);
		static void OnRuntimeStop(Scene& scene);
	};

}
