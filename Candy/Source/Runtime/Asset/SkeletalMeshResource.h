#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/Asset/MeshData.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Candy {

	// =========================================================================
	// Skeletal animation data (asset layer) — imported from glTF skins +
	// animations by MeshImporter. Runtime animation evaluation (sampling +
	// joint hierarchy accumulation) lives in SkeletalAnimationSystem.
	// =========================================================================

	struct SkeletonJoint
	{
		std::string Name;
		int32_t     ParentIndex = -1;                    // -1 = root
		glm::mat4   LocalRestPose = glm::mat4(1.0f);     // bind-pose local TRS
		glm::mat4   InverseBindMatrix = glm::mat4(1.0f); // mesh space -> joint space
	};

	enum class AnimPath : uint8_t { Translation, Rotation, Scale };
	enum class AnimInterp : uint8_t { Linear, Step, CubicSpline };

	struct AnimationTrack
	{
		uint32_t JointIndex = 0;                 // into Skeleton
		AnimPath  Path   = AnimPath::Translation;
		AnimInterp Interp = AnimInterp::Linear;
		std::vector<float>     Times;           // keyframe times (seconds)
		// T/S: vec4(x,y,z) per key; R: quat(x,y,z,w) per key.
		// CubicSpline: (inTangent, value, outTangent) triples, N*3 entries.
		std::vector<glm::vec4> Values;
	};

	struct AnimationClip
	{
		std::string Name;
		float Duration = 0.0f;
		std::vector<AnimationTrack> Tracks;
	};

	class SkeletalMeshResource
	{
	public:
		std::vector<SkinnedMeshVertex> Vertices;
		std::vector<uint32_t>          Indices;
		std::vector<Submesh>           Submeshes;
		MeshAABB                       Bounds;

		// Joints are stored in topological order (parent before child) so the
		// runtime evaluator can accumulate global poses in a single pass.
		std::vector<SkeletonJoint> Skeleton;
		std::vector<AnimationClip> Clips;

		bool IsSkinned() const { return !Skeleton.empty(); }

		void RecalculateBounds();
	};

} // namespace Candy
