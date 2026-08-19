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
	// matrices and uploads the result into a per-skeleton bone CB (b3) cached
	// here. SceneRenderer binds that CB for each skinned draw.
	// =========================================================================
	class SkeletalAnimationSystem
	{
	public:
		/// Advances every SkeletalMeshComponent's Time by dt (when Play) and
		/// re-evaluates its pose, writing the bone CB and DebugGlobalPose.
		static void Update(Scene& scene, float dt);

		/// Lazy-created per-skeleton bone CB (root CBV, 256B-aligned).
		/// Returns nullptr when the skeleton has not been evaluated yet.
		static Ref<RHIBuffer> GetBoneBuffer(const Ref<SkeletalMeshResource>& mesh);

		/// Releases all cached bone CBs (called on renderer shutdown).
		static void Shutdown();

	private:
		struct SkeletonGPU
		{
			Ref<RHIBuffer> Buffer;
			uint32_t Joints = 0;
		};
		static std::unordered_map<const SkeletalMeshResource*, SkeletonGPU> s_Skeletons;
	};

} // namespace Candy
