#include "CandyPCH.h"

#include "Runtime/Asset/MeshImporter.h"
#include "Runtime/Core/FileSystem.h"

#include "cgltf.h" // declarations only; implementation lives in CgltfImpl.cpp

#include <cstdlib>
#include <cstring>
#include <glm/glm.hpp>

namespace Candy {

	namespace {

		// ---------------------------------------------------------------------
		// VFS-backed cgltf file.read callback. cgltf passes a VFS:// path (it
		// concatenates the gltf path's directory with the buffer URI), which we
		// resolve through the FileSystem. cgltf will later release the buffer
		// with its memory.free_func (default free), so we allocate with malloc.
		// ---------------------------------------------------------------------
		cgltf_result VfsFileRead(const cgltf_memory_options* /*memory*/,
			const cgltf_file_options* /*file*/, const char* path,
			cgltf_size* size, void** data)
		{
			std::optional<std::vector<uint8_t>> bytes = FileSystem::Get().Read(path);
			if (!bytes)
				return cgltf_result_file_not_found;

			void* buffer = std::malloc(bytes->size() > 0 ? bytes->size() : 1);
			if (!buffer)
				return cgltf_result_out_of_memory;

			std::memcpy(buffer, bytes->data(), bytes->size());
			*data = buffer;
			if (size)
				*size = bytes->size();
			return cgltf_result_success;
		}

		// Directory part of a VFS path, including trailing slash.
		// "VFS://Game/Model/lemon/lemon_1k.gltf" -> "VFS://Game/Model/lemon/"
		std::string GetVfsDirectory(const std::string& path)
		{
			const size_t pos = path.find_last_of('/');
			if (pos == std::string::npos)
				return "";
			return path.substr(0, pos + 1);
		}

		// Resolve a glTF texture/image URI to an absolute VFS path.
		std::string TextureVfsPath(const cgltf_texture_view& view, const std::string& baseDir)
		{
			if (!view.texture || !view.texture->image || !view.texture->image->uri)
				return "";
			return baseDir + view.texture->image->uri;
		}

		Ref<Material> ImportMaterial(const cgltf_material* mat, const std::string& baseDir)
		{
			Ref<Material> material = CreateRef<Material>();
			if (mat->name)
				material->Name = mat->name;

			// glTF PBR data is imported into conventional parameter keys
			// ("u_BaseColor", ...) so it surfaces in the inspector whenever a
			// bound shader declares matching //@param uniforms.
			if (mat->has_pbr_metallic_roughness)
			{
				const cgltf_pbr_metallic_roughness& pbr = mat->pbr_metallic_roughness;
				material->ShaderParams["u_BaseColor"] = glm::vec4(
					pbr.base_color_factor[0], pbr.base_color_factor[1],
					pbr.base_color_factor[2], pbr.base_color_factor[3]);
				material->ShaderParams["u_Metallic"]  = pbr.metallic_factor;
				material->ShaderParams["u_Roughness"] = pbr.roughness_factor;

				const std::string baseColorMap = TextureVfsPath(pbr.base_color_texture, baseDir);
				if (!baseColorMap.empty())
					material->ShaderParams["u_BaseColorMap"] = baseColorMap;
				const std::string metalRoughMap = TextureVfsPath(pbr.metallic_roughness_texture, baseDir);
				if (!metalRoughMap.empty())
					material->ShaderParams["u_MetallicRoughnessMap"] = metalRoughMap;
			}

			material->ShaderParams["u_Emissive"] = glm::vec3(
				mat->emissive_factor[0], mat->emissive_factor[1], mat->emissive_factor[2]);
			if (mat->has_emissive_strength)
				material->ShaderParams["u_EmissiveIntensity"] = mat->emissive_strength.emissive_strength;

			const std::string normalMap = TextureVfsPath(mat->normal_texture, baseDir);
			if (!normalMap.empty())
				material->ShaderParams["u_NormalMap"] = normalMap;
			const std::string emissiveMap = TextureVfsPath(mat->emissive_texture, baseDir);
			if (!emissiveMap.empty())
				material->ShaderParams["u_EmissiveMap"] = emissiveMap;

			// 0 = Opaque, 1 = Masked, 2 = Transparent (matches cgltf_alpha_mode).
			int blendMode = static_cast<int>(mat->alpha_mode);
			if (blendMode > 2 || blendMode < 0)
				blendMode = 0;
			material->ShaderParams["u_BlendMode"] = blendMode;

			material->ShaderParams["u_ShadingModel"] = mat->unlit ? 1 : 0;

			// Bind the engine's built-in PBR shader by default so the imported
			// parameters (u_BaseColor, ...) are immediately editable in the
			// material inspector. Users can override it with any shader.
			material->ShaderPath = "VFS://Engine/Content/Shaders/D3D12/PBR.hlsl";

			return material;
		}

		int FindMaterialIndex(const std::vector<Ref<Material>>& materials, const std::string& name)
		{
			for (size_t i = 0; i < materials.size(); i++)
			{
				if (materials[i]->Name == name)
					return static_cast<int>(i);
			}
			return -1;
		}

		void ImportPrimitive(Ref<StaticMeshResource>& mesh,
			std::vector<Ref<Material>>& materials,
			const cgltf_primitive& prim, const std::string& baseDir,
			const std::string& meshName)
		{
			// Collect the attributes we care about.
			const cgltf_accessor* posAccessor = nullptr;
			const cgltf_accessor* norAccessor = nullptr;
			const cgltf_accessor* texAccessor = nullptr;
			const cgltf_accessor* tanAccessor = nullptr;
			for (cgltf_size a = 0; a < prim.attributes_count; a++)
			{
				const cgltf_attribute& attr = prim.attributes[a];
				switch (attr.type)
				{
					case cgltf_attribute_type_position: posAccessor = attr.data; break;
					case cgltf_attribute_type_normal:   norAccessor = attr.data; break;
					case cgltf_attribute_type_texcoord: if (attr.index == 0) texAccessor = attr.data; break;
					case cgltf_attribute_type_tangent:  tanAccessor = attr.data; break;
					default: break;
				}
			}

			if (!posAccessor)
			{
				CANDY_CORE_WARN("MeshImporter: primitive in '{}' has no POSITION attribute, skipping", meshName);
				return;
			}

			const cgltf_size vertexCount = posAccessor->count;
			const uint32_t baseVertex = static_cast<uint32_t>(mesh->Vertices.size());

			// Unpack attribute streams into flat float arrays.
			std::vector<float> positions(vertexCount * 3);
			cgltf_accessor_unpack_floats(posAccessor, positions.data(), positions.size());

			std::vector<float> normals;
			if (norAccessor)
			{
				normals.resize(vertexCount * 3);
				cgltf_accessor_unpack_floats(norAccessor, normals.data(), normals.size());
			}

			std::vector<float> texcoords;
			if (texAccessor)
			{
				texcoords.resize(vertexCount * 2);
				cgltf_accessor_unpack_floats(texAccessor, texcoords.data(), texcoords.size());
			}

			std::vector<float> tangents;
			if (tanAccessor)
			{
				tangents.resize(vertexCount * 4);
				cgltf_accessor_unpack_floats(tanAccessor, tangents.data(), tangents.size());
			}

			// Build vertices.
			mesh->Vertices.reserve(mesh->Vertices.size() + vertexCount);
			for (cgltf_size i = 0; i < vertexCount; i++)
			{
				MeshVertex v;
				v.Position = glm::vec3(
					positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]);

				if (!normals.empty())
					v.Normal = glm::vec3(normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]);
				if (!texcoords.empty())
					v.TexCoord = glm::vec2(texcoords[i * 2 + 0], texcoords[i * 2 + 1]);
				if (!tangents.empty())
					v.Tangent = glm::vec4(tangents[i * 4 + 0], tangents[i * 4 + 1], tangents[i * 4 + 2], tangents[i * 4 + 3]);

				mesh->Vertices.push_back(v);
			}

			// Build indices (indexed or implicit).
			std::vector<uint32_t> indices;
			if (prim.indices)
			{
				const cgltf_size indexCount = prim.indices->count;
				indices.resize(indexCount);
				cgltf_accessor_unpack_indices(prim.indices, indices.data(), sizeof(uint32_t), indexCount);
			}
			else
			{
				indices.resize(vertexCount);
				for (cgltf_size i = 0; i < vertexCount; i++)
					indices[i] = static_cast<uint32_t>(i);
			}

			const uint32_t indexOffset = static_cast<uint32_t>(mesh->Indices.size());
			mesh->Indices.reserve(mesh->Indices.size() + indices.size());
			for (uint32_t idx : indices)
				mesh->Indices.push_back(idx + baseVertex);

			// Submesh.
			Submesh submesh;
			submesh.Name = meshName.empty() ? "Mesh" : meshName;
			submesh.IndexOffset   = indexOffset;
			submesh.IndexCount    = static_cast<uint32_t>(indices.size());
			submesh.MaterialIndex = 0;

			// Material (deduplicated by name).
			if (prim.material)
			{
				Ref<Material> material = ImportMaterial(prim.material, baseDir);
				int materialIndex = FindMaterialIndex(materials, material->Name);
				if (materialIndex < 0)
				{
					materials.push_back(material);
					materialIndex = static_cast<int>(materials.size()) - 1;
				}
				submesh.MaterialIndex = static_cast<uint32_t>(materialIndex);
				submesh.MaterialName   = material->Name;
			}

			mesh->Submeshes.push_back(submesh);
		}

	} // namespace

	Ref<ImportedStaticMesh> MeshImporter::ImportStaticMesh(const std::string& gltfPath)
	{
		std::optional<std::vector<uint8_t>> gltfBytes = FileSystem::Get().Read(gltfPath);
		if (!gltfBytes)
		{
			CANDY_CORE_ERROR("MeshImporter: failed to read glTF file: {}", gltfPath);
			return nullptr;
		}

		cgltf_options options = {};
		options.file.read = VfsFileRead;

		cgltf_data* cgltfData = nullptr;
		cgltf_result result = cgltf_parse(&options, gltfBytes->data(), gltfBytes->size(), &cgltfData);
		if (result != cgltf_result_success)
		{
			CANDY_CORE_ERROR("MeshImporter: cgltf_parse failed ({}) for {}", static_cast<int>(result), gltfPath);
			return nullptr;
		}

		// Load external .bin buffers via the VFS file.read callback.
		// Passing the gltf VFS path lets cgltf derive buffer paths relative to it.
		result = cgltf_load_buffers(&options, cgltfData, gltfPath.c_str());
		if (result != cgltf_result_success)
		{
			CANDY_CORE_ERROR("MeshImporter: cgltf_load_buffers failed ({}) for {}", static_cast<int>(result), gltfPath);
			cgltf_free(cgltfData);
			return nullptr;
		}

		const std::string baseDir = GetVfsDirectory(gltfPath);

		Ref<ImportedStaticMesh> imported = CreateRef<ImportedStaticMesh>();
		imported->Mesh = CreateRef<StaticMeshResource>();

		for (cgltf_size m = 0; m < cgltfData->meshes_count; m++)
		{
			const cgltf_mesh& cgltfMesh = cgltfData->meshes[m];
			const std::string meshName = cgltfMesh.name ? cgltfMesh.name : "";
			for (cgltf_size p = 0; p < cgltfMesh.primitives_count; p++)
			{
				const cgltf_primitive& prim = cgltfMesh.primitives[p];
				if (prim.type != cgltf_primitive_type_triangles)
				{
					CANDY_CORE_WARN("MeshImporter: skipping non-triangle primitive in '{}'", meshName);
					continue;
				}
				ImportPrimitive(imported->Mesh, imported->Materials, prim, baseDir, meshName);
			}
		}

		if (imported->Mesh->Vertices.empty())
		{
			CANDY_CORE_ERROR("MeshImporter: no importable geometry found in {}", gltfPath);
			cgltf_free(cgltfData);
			return nullptr;
		}

		imported->Mesh->RecalculateBounds();
		cgltf_free(cgltfData);

		CANDY_CORE_INFO("MeshImporter: imported '{}' ({} verts, {} indices, {} materials)",
			gltfPath,
			imported->Mesh->Vertices.size(),
			imported->Mesh->Indices.size(),
			imported->Materials.size());

		return imported;
	}

} // namespace Candy
