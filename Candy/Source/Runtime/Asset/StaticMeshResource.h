#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/Asset/MeshData.h"

#include <cstdint>
#include <vector>

namespace Candy {

	// =========================================================================
	// StaticMeshResource — CPU-side static mesh data (asset layer)
	//
	// Owns vertex/index/submesh data produced by MeshImporter (glTF) or the
	// built-in procedural generators (cube/sphere/plane). No skinning data —
	// skeletal meshes will use a separate SkeletalMeshResource. The GPU-side
	// counterpart (RHIBuffer VB/IB) is Renderer::Mesh, built from this.
	// =========================================================================

	class StaticMeshResource
	{
	public:
		StaticMeshResource() = default;
		~StaticMeshResource() = default;

		std::vector<MeshVertex> Vertices;
		std::vector<uint32_t>   Indices;
		std::vector<Submesh>    Submeshes;
		MeshAABB                Bounds;

		void RecalculateBounds();

		// Built-in procedural meshes (no external asset required)
		static Ref<StaticMeshResource> CreateCube(float size = 1.0f);
		static Ref<StaticMeshResource> CreateSphere(float radius = 0.5f, uint32_t segments = 32, uint32_t rings = 16);
		static Ref<StaticMeshResource> CreatePlane(float size = 1.0f);
		/// 1×1 quad centered at the origin, XY plane, +Z normal, UV 0..1.
		/// Shared by sprites and SDF circles; size/rotation/position come from
		/// TransformComponent at submit time.
		static Ref<StaticMeshResource> CreateQuad();
	};

} // namespace Candy
