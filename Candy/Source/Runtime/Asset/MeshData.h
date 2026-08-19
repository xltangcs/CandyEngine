#pragma once

#include "Runtime/Core/Base.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>

namespace Candy {

	// =========================================================================
	// MeshData — shared CPU-side mesh primitives (asset layer)
	//
	// Used by StaticMeshResource and (later) SkeletalMeshResource. The GPU-side
	// counterpart (RHIBuffer VB/IB) is Renderer::Mesh.
	//
	// Vertex layout (pos/normal/tangent/uv) matches G-buffer requirements so
	// the same source feeds both forward and deferred paths later.
	// =========================================================================

	struct MeshVertex
	{
		glm::vec3 Position = glm::vec3(0.0f);
		glm::vec3 Normal   = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec4 Tangent  = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // w = bitangent sign
		glm::vec2 TexCoord = glm::vec2(0.0f);
	};

	// =========================================================================
	// SkinnedMeshVertex — skinned mesh vertex (asset layer)
	//
	// BoneIndices/Weights drive GPU skinning (SkinnedVSMain in PBR.hlsl).
	// JOINTS_0 is stored as 4 × uint8 (glTF u8/u16 compressed data is converted
	// at import time); weights are normalized at import time.
	// =========================================================================
	struct SkinnedMeshVertex
	{
		glm::vec3 Position = glm::vec3(0.0f);
		glm::vec3 Normal   = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec4 Tangent  = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // w = bitangent sign
		glm::vec2 TexCoord = glm::vec2(0.0f);
		uint8_t   BoneIndices[4] = { 0, 0, 0, 0 };
		float     BoneWeights[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
	};
	static_assert(sizeof(SkinnedMeshVertex) == 68, "SkinnedMeshVertex layout must stay 68B (offsets: pos 0, nrm 12, tan 24, uv 40, bones 48, weights 52)");

	struct Submesh
	{
		std::string Name;
		uint32_t    IndexOffset   = 0;
		uint32_t    IndexCount    = 0;
		uint32_t    MaterialIndex = 0;         // slot into material list
		std::string MaterialName;              // glTF material name (for .mat generation)
	};

	struct MeshAABB
	{
		glm::vec3 Min = glm::vec3(0.0f);
		glm::vec3 Max = glm::vec3(0.0f);

		glm::vec3 GetCenter() const { return (Min + Max) * 0.5f; }
		glm::vec3 GetExtent() const { return (Max - Min) * 0.5f; }
	};

} // namespace Candy
