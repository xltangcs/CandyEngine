#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/Asset/SkeletalMeshResource.h"

#include <unordered_map>

namespace Candy {

	class RHIBuffer;
	class Scene;

	// =========================================================================
	// SkeletalAnimationSystem — per-frame animation evaluation for every
	// SkeletalMeshComponent. Advances Time, samples the active clip (linear /
	// step / cubic-spline), accumulates global joint poses (joints are stored
	// in topological order, so one pass suffices), applies inverse bind
	// matrices and uploads the result into the component's per-instance bone
	// CB (b3). SceneRenderer binds that CB for each skinned draw.
	// =========================================================================
	class SkeletalAnimationSystem
	{
	public:
		/// Advances every SkeletalMeshComponent's Time by dt (when Play) and
		/// re-evaluates its pose, writing the component's bone CB and
		/// DebugGlobalPose.
		static void Update(Scene& scene, float dt);
	};

} // namespace Candy
