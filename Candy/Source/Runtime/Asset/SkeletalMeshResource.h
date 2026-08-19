#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/Asset/MeshData.h"
#include "Runtime/Asset/SkeletonResource.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Candy {

	// =========================================================================
	// SkeletalMeshResource — skinned mesh geometry (asset layer). The rig and
	// animation clips live in SkeletonResource; a mesh merely references it.
	// =========================================================================

	class SkeletalMeshResource
	{
	public:
		std::vector<SkinnedMeshVertex> Vertices;
		std::vector<uint32_t>          Indices;
		std::vector<Submesh>           Submeshes;
		MeshAABB                       Bounds;

		// The rig this mesh is skinned to (shared across meshes using the
		// same skeleton). May be null for unskinned meshes.
		Ref<SkeletonResource> Skeleton;

		bool IsSkinned() const { return Skeleton && !Skeleton->Joints.empty(); }

		void RecalculateBounds();
	};

} // namespace Candy
