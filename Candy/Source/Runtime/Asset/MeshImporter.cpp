#include "CandyPCH.h"

#include "Runtime/Asset/MeshImporter.h"
#include "Runtime/Core/FileSystem.h"

#include "cgltf.h" // declarations only; implementation lives in CgltfImpl.cpp

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <glm/glm.hpp>
#include <unordered_map>

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

	// =========================================================================
	// Skeletal mesh import helpers
	// =========================================================================

	namespace {

		// Builds the skeleton from a glTF skin. glTF's skin.joints order is
		// arbitrary, so joints are reordered to topological order (parent before
		// child); `oldToNew` maps original skin indices to reordered ones (vertex
		// JOINTS_0 values and animation channel targets must be remapped through
		// it). `nodeToIndex` maps glTF nodes in the skin to their *original*
		// skin indices (used for animation channel targets).
		std::vector<SkeletonJoint> BuildSkeleton(const cgltf_skin& skin,
			std::unordered_map<const cgltf_node*, int32_t>& nodeToIndex,
			std::vector<int32_t>& oldToNew)
		{
			const cgltf_size count = skin.joints_count;
			for (cgltf_size i = 0; i < count; i++)
				nodeToIndex[skin.joints[i]] = static_cast<int32_t>(i);

			// Parent indices in the *original* skin order.
			std::vector<int32_t> parentOf(count, -1);
			for (cgltf_size i = 0; i < count; i++)
			{
				const cgltf_node* parent = skin.joints[i]->parent;
				if (!parent)
					continue;
				auto it = nodeToIndex.find(parent);
				if (it != nodeToIndex.end())
					parentOf[i] = it->second;
			}

			// Topological order (parents first) via recursive chain walk.
			std::vector<int32_t> order;
			order.reserve(count);
			std::vector<bool> visited(count, false);
			std::function<void(int32_t)> visit = [&](int32_t j) {
				if (visited[j])
					return;
				if (parentOf[j] >= 0)
					visit(parentOf[j]);
				visited[j] = true;
				order.push_back(j);
			};
			for (cgltf_size i = 0; i < count; i++)
				visit(static_cast<int32_t>(i));

			oldToNew.assign(count, -1);
			for (int32_t i = 0; i < static_cast<int32_t>(order.size()); i++)
				oldToNew[order[i]] = i;

			std::vector<SkeletonJoint> joints(order.size());
			for (int32_t i = 0; i < static_cast<int32_t>(order.size()); i++)
			{
				const cgltf_node* node = skin.joints[order[i]];
				SkeletonJoint& j = joints[i];
				j.Name = node->name ? node->name : "";
				const int32_t origParent = parentOf[order[i]];
				j.ParentIndex = origParent >= 0 ? oldToNew[origParent] : -1;

				// Local rest pose from the node's TRS or matrix (column-major,
				// matches GLM).
				cgltf_float m[16];
				cgltf_node_transform_local(node, m);
				std::memcpy(&j.LocalRestPose, m, sizeof(m));

				if (skin.inverse_bind_matrices && skin.inverse_bind_matrices->count > order[i])
				{
					cgltf_float ibm[16];
					if (cgltf_accessor_read_float(skin.inverse_bind_matrices, order[i], ibm, 16))
						std::memcpy(&j.InverseBindMatrix, ibm, sizeof(ibm));
				}
			}
			return joints;
		}

		// Imports one skinned primitive (JOINTS_0/WEIGHTS_0 required). Vertex
		// bone indices are remapped through `oldToNew` (skin order -> topological
		// order); weights are normalized.
		void ImportSkinnedPrimitive(Ref<SkeletalMeshResource>& mesh,
			std::vector<Ref<Material>>& materials,
			const cgltf_primitive& prim, const std::string& baseDir,
			const std::string& meshName, const std::vector<int32_t>& oldToNew)
		{
			const cgltf_accessor* posAccessor = nullptr;
			const cgltf_accessor* norAccessor = nullptr;
			const cgltf_accessor* texAccessor = nullptr;
			const cgltf_accessor* tanAccessor = nullptr;
			const cgltf_accessor* jointAccessor = nullptr;
			const cgltf_accessor* weightAccessor = nullptr;
			for (cgltf_size a = 0; a < prim.attributes_count; a++)
			{
				const cgltf_attribute& attr = prim.attributes[a];
				switch (attr.type)
				{
					case cgltf_attribute_type_position: posAccessor = attr.data; break;
					case cgltf_attribute_type_normal:   norAccessor = attr.data; break;
					case cgltf_attribute_type_texcoord: if (attr.index == 0) texAccessor = attr.data; break;
					case cgltf_attribute_type_tangent:  tanAccessor = attr.data; break;
					case cgltf_attribute_type_joints:   if (attr.index == 0) jointAccessor = attr.data; break;
					case cgltf_attribute_type_weights:  if (attr.index == 0) weightAccessor = attr.data; break;
					default: break;
				}
			}

			if (!posAccessor)
			{
				CANDY_CORE_WARN("MeshImporter: primitive in '{}' has no POSITION attribute, skipping", meshName);
				return;
			}
			if (!jointAccessor)
			{
				CANDY_CORE_WARN("MeshImporter: skinned primitive in '{}' has no JOINTS_0 attribute, skipping", meshName);
				return;
			}

			const cgltf_size vertexCount = posAccessor->count;
			const uint32_t baseVertex = static_cast<uint32_t>(mesh->Vertices.size());

			// Unpack attribute streams into flat arrays.
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

			// JOINTS_0 may be u8/u16 compressed; cgltf converts on read.
			std::vector<cgltf_uint> joints(vertexCount * 4, 0);
			for (cgltf_size i = 0; i < vertexCount; i++)
				cgltf_accessor_read_uint(jointAccessor, i, &joints[i * 4], 4);

			std::vector<float> weights(vertexCount * 4, 0.0f);
			if (weightAccessor)
			{
				weights.resize(vertexCount * 4);
				cgltf_accessor_unpack_floats(weightAccessor, weights.data(), weights.size());
			}
			else
			{
				for (cgltf_size i = 0; i < vertexCount; i++)
					weights[i * 4] = 1.0f;
			}

			// Build vertices.
			mesh->Vertices.reserve(mesh->Vertices.size() + vertexCount);
			for (cgltf_size i = 0; i < vertexCount; i++)
			{
				SkinnedMeshVertex v;
				v.Position = glm::vec3(
					positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]);

				if (!normals.empty())
					v.Normal = glm::vec3(normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]);
				if (!texcoords.empty())
					v.TexCoord = glm::vec2(texcoords[i * 2 + 0], texcoords[i * 2 + 1]);
				if (!tangents.empty())
					v.Tangent = glm::vec4(tangents[i * 4 + 0], tangents[i * 4 + 1], tangents[i * 4 + 2], tangents[i * 4 + 3]);

				float sum = 0.0f;
				for (int k = 0; k < 4; k++)
				{
				const uint32_t j = joints[i * 4 + k];
				CANDY_CORE_ASSERT(j < oldToNew.size() && oldToNew[j] <= 255, "joint index exceeds uint8 JOINTS_0 capacity");
				v.BoneIndices[k] = (j < oldToNew.size()) ? static_cast<uint8_t>(oldToNew[j]) : 0;
					v.BoneWeights[k] = weights[i * 4 + k];
					sum += v.BoneWeights[k];
				}
				if (sum > 1e-4f)
				{
					for (int k = 0; k < 4; k++)
						v.BoneWeights[k] /= sum;
				}
				else
				{
					v.BoneIndices[0] = 0;
					v.BoneWeights[0] = 1.0f;
					for (int k = 1; k < 4; k++)
					{
						v.BoneIndices[k] = 0;
						v.BoneWeights[k] = 0.0f;
					}
				}

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

		// Converts glTF animations into engine clips. Channels targeting nodes
		// outside the skinned skeleton are skipped (scene-level node animation
		// is out of scope); morph target (weights) channels are skipped too.
		std::vector<AnimationClip> BuildAnimationClips(const cgltf_data& data,
			const std::unordered_map<const cgltf_node*, int32_t>& nodeToIndex,
			const std::vector<int32_t>& oldToNew)
		{
			std::vector<AnimationClip> clips;
			clips.reserve(data.animations_count);
			for (cgltf_size a = 0; a < data.animations_count; a++)
			{
				const cgltf_animation& anim = data.animations[a];
				AnimationClip clip;
				clip.Name = anim.name ? anim.name : ("Clip" + std::to_string(a));

				for (cgltf_size c = 0; c < anim.channels_count; c++)
				{
					const cgltf_animation_channel& ch = anim.channels[c];

					if (ch.target_path == cgltf_animation_path_type_weights)
					{
						CANDY_CORE_WARN("MeshImporter: morph target animation channel skipped (not supported)");
						continue;
					}

					auto it = nodeToIndex.find(ch.target_node);
					if (it == nodeToIndex.end())
						continue; // node not part of the skinned skeleton

					AnimPath path;
					uint32_t comps;
					switch (ch.target_path)
					{
						case cgltf_animation_path_type_translation: path = AnimPath::Translation; comps = 3; break;
						case cgltf_animation_path_type_rotation:    path = AnimPath::Rotation;    comps = 4; break;
						case cgltf_animation_path_type_scale:       path = AnimPath::Scale;       comps = 3; break;
						default: continue;
					}

					AnimInterp interp;
					switch (ch.sampler->interpolation)
					{
						case cgltf_interpolation_type_step:         interp = AnimInterp::Step; break;
						case cgltf_interpolation_type_cubic_spline: interp = AnimInterp::CubicSpline; break;
						case cgltf_interpolation_type_linear:       interp = AnimInterp::Linear; break;
						default: continue;
					}

					const cgltf_accessor* input = ch.sampler->input;
					const cgltf_accessor* output = ch.sampler->output;
					if (!input || !output || input->count == 0 || output->count == 0)
						continue;

					AnimationTrack track;
					track.JointIndex = static_cast<uint32_t>(oldToNew[it->second]);
					track.Path   = path;
					track.Interp = interp;

					track.Times.resize(input->count);
					cgltf_accessor_unpack_floats(input, track.Times.data(), track.Times.size());

					// accessor->count is the *element* count (VEC4s / VEC3s),
					// not the float count; unpack_floats writes count*comps floats.
					std::vector<float> raw(output->count * comps);
					cgltf_accessor_unpack_floats(output, raw.data(), raw.size());

					// Cubic spline stores (inTangent, value, outTangent) triples
					// per keyframe; linear/step store the value only. keyCount
					// is in elements, so divide by triple only.
					const uint32_t triple = (interp == AnimInterp::CubicSpline) ? 3u : 1u;
					const cgltf_size keyCount = output->count / triple;
					track.Values.resize(keyCount * triple);
					for (cgltf_size k = 0; k < keyCount; k++)
					{
						for (uint32_t t = 0; t < triple; t++)
						{
							for (uint32_t e = 0; e < comps; e++)
								track.Values[k * triple + t][e] = raw[(k * triple + t) * comps + e];
						}
					}

					if (!track.Times.empty())
						clip.Duration = std::max(clip.Duration, track.Times.back());

					clip.Tracks.push_back(std::move(track));
				}

				if (!clip.Tracks.empty())
					clips.push_back(std::move(clip));
			}
			return clips;
		}

	} // namespace

	Ref<ImportedSkeletalMesh> MeshImporter::ImportSkeletalMesh(const std::string& gltfPath)
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

		result = cgltf_load_buffers(&options, cgltfData, gltfPath.c_str());
		if (result != cgltf_result_success)
		{
			CANDY_CORE_ERROR("MeshImporter: cgltf_load_buffers failed ({}) for {}", static_cast<int>(result), gltfPath);
			cgltf_free(cgltfData);
			return nullptr;
		}

		if (cgltfData->skins_count == 0 || cgltfData->skins[0].joints_count == 0)
		{
			CANDY_CORE_ERROR("MeshImporter: '{}' has no skin; use ImportStaticMesh instead", gltfPath);
			cgltf_free(cgltfData);
			return nullptr;
		}
		if (cgltfData->skins_count > 1)
			CANDY_CORE_WARN("MeshImporter: '{}' has {} skins; only the first is imported", gltfPath, cgltfData->skins_count);

		const std::string baseDir = GetVfsDirectory(gltfPath);

		std::unordered_map<const cgltf_node*, int32_t> nodeToIndex;
		std::vector<int32_t> oldToNew;
		std::vector<SkeletonJoint> skeleton = BuildSkeleton(cgltfData->skins[0], nodeToIndex, oldToNew);
		if (skeleton.size() > 255)
		{
			CANDY_CORE_ERROR("MeshImporter: '{}' has {} joints; JOINTS_0 is uint8 (max 255), import aborted",
				gltfPath, skeleton.size());
			cgltf_free(cgltfData);
			return nullptr;
		}

		Ref<ImportedSkeletalMesh> imported = CreateRef<ImportedSkeletalMesh>();
		imported->Mesh = CreateRef<SkeletalMeshResource>();
		imported->Mesh->Skeleton = std::move(skeleton);

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
				ImportSkinnedPrimitive(imported->Mesh, imported->Materials, prim, baseDir, meshName, oldToNew);
			}
		}

		if (imported->Mesh->Vertices.empty())
		{
			CANDY_CORE_ERROR("MeshImporter: no skinned geometry found in {}", gltfPath);
			cgltf_free(cgltfData);
			return nullptr;
		}

		imported->Mesh->Clips = BuildAnimationClips(*cgltfData, nodeToIndex, oldToNew);
		imported->Mesh->RecalculateBounds();
		cgltf_free(cgltfData);

		CANDY_CORE_INFO("MeshImporter: imported skinned '{}' ({} verts, {} joints, {} clips, {} materials)",
			gltfPath,
			imported->Mesh->Vertices.size(),
			imported->Mesh->Skeleton.size(),
			imported->Mesh->Clips.size(),
			imported->Materials.size());
		for (const auto& clip : imported->Mesh->Clips)
			CANDY_CORE_INFO("MeshImporter:   clip '{}' duration {:.2f}s ({} tracks)",
				clip.Name, clip.Duration, clip.Tracks.size());

		return imported;
	}

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
