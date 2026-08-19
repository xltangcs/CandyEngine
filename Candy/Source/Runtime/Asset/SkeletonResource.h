#pragma once

#include "Runtime/Core/Base.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Candy {

	// =========================================================================
	// SkeletonResource — the rig (joint hierarchy) plus its animation clips,
	// imported from glTF skins + animations by MeshImporter. Multiple
	// SkeletalMeshResources (different geometry, e.g. LODs or characters
	// sharing a rig) can reference the same SkeletonResource.
	//
	// Joints are stored in topological order (parent before child) so the
	// runtime evaluator can accumulate global poses in a single pass.
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
		uint32_t JointIndex = 0;                 // into SkeletonResource::Joints
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

	class SkeletonResource
	{
	public:
		std::vector<SkeletonJoint> Joints;
		std::vector<AnimationClip> Clips;
	};

} // namespace Candy
