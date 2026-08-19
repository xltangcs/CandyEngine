#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/Asset/StaticMeshResource.h"
#include "Runtime/Asset/SkeletalMeshResource.h"
#include "Runtime/Asset/Material.h"

#include <string>
#include <vector>

namespace Candy {

	// Result of importing static meshes from a glTF file.
	// Mesh's Submeshes reference Materials by index (Submesh::MaterialIndex).
	struct ImportedStaticMesh
	{
		Ref<StaticMeshResource> Mesh;
		std::vector<Ref<Material>> Materials;
	};

	// Result of importing a skinned glTF file (JOINTS_0/WEIGHTS_0 attributes +
	// skin + animations).
	struct ImportedSkeletalMesh
	{
		Ref<SkeletalMeshResource> Mesh;
		std::vector<Ref<Material>> Materials;
	};

	class MeshImporter
	{
	public:
		// Imports all meshes/primitives from a .gltf file into a single
		// StaticMeshResource; each glTF primitive becomes one Submesh, and the
		// associated glTF materials are converted to Material assets.
		// gltfPath is a VFS:// path. Returns nullptr on failure.
		static Ref<ImportedStaticMesh> ImportStaticMesh(const std::string& gltfPath);

		// Imports a skinned .gltf file (mesh + skin skeleton + animation
		// clips) into a SkeletalMeshResource. The skeleton is reordered to
		// topological order (parent before child) with vertex JOINTS_0 indices
		// remapped accordingly. Returns nullptr on failure.
		static Ref<ImportedSkeletalMesh> ImportSkeletalMesh(const std::string& gltfPath);
	};

} // namespace Candy
