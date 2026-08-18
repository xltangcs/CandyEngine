#include "CandyPCH.h"
#include "Runtime/Asset/StaticMeshResource.h"

#include <glm/gtc/constants.hpp>

namespace Candy {

	void StaticMeshResource::RecalculateBounds()
	{
		if (Vertices.empty())
		{
			Bounds = {};
			return;
		}

		glm::vec3 minPos = Vertices[0].Position;
		glm::vec3 maxPos = Vertices[0].Position;
		for (const auto& v : Vertices)
		{
			minPos = glm::min(minPos, v.Position);
			maxPos = glm::max(maxPos, v.Position);
		}
		Bounds.Min = minPos;
		Bounds.Max = maxPos;
	}

	Ref<StaticMeshResource> StaticMeshResource::CreateCube(float size)
	{
		Ref<StaticMeshResource> source = CreateRef<StaticMeshResource>();
		const float h = size * 0.5f;

		// 6 faces × 4 verts = 24 verts (flat normals/tangents per face)
		struct FaceDef { glm::vec3 Normal; glm::vec3 Tangent; glm::vec3 Axis0; glm::vec3 Axis1; };
		const FaceDef faces[6] = {
			{ { 1, 0, 0 }, { 0, 0,-1 }, { 0, 0,-1 }, { 0, 1, 0 } },  // +X
			{ {-1, 0, 0 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 1, 0 } },  // -X
			{ { 0, 1, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { 0, 0,-1 } },  // +Y
			{ { 0,-1, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },  // -Y
			{ { 0, 0, 1 }, { 1, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } },  // +Z
			{ { 0, 0,-1 }, {-1, 0, 0 }, {-1, 0, 0 }, { 0, 1, 0 } },  // -Z
		};
		const glm::vec2 uvs[4] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };

		source->Vertices.reserve(24);
		source->Indices.reserve(36);

		for (const FaceDef& face : faces)
		{
			uint32_t baseVertex = static_cast<uint32_t>(source->Vertices.size());
			glm::vec3 center = face.Normal * h;
			// Quad corners: (-a0-a1), (+a0-a1), (+a0+a1), (-a0+a1)
			const glm::vec3 corners[4] = {
				center - face.Axis0 * h - face.Axis1 * h,
				center + face.Axis0 * h - face.Axis1 * h,
				center + face.Axis0 * h + face.Axis1 * h,
				center - face.Axis0 * h + face.Axis1 * h,
			};
			for (int i = 0; i < 4; i++)
			{
				MeshVertex v;
				v.Position = corners[i];
				v.Normal = face.Normal;
				v.Tangent = glm::vec4(face.Tangent, 1.0f);
				v.TexCoord = uvs[i];
				source->Vertices.push_back(v);
			}
			source->Indices.insert(source->Indices.end(), {
				baseVertex + 0, baseVertex + 1, baseVertex + 2,
				baseVertex + 0, baseVertex + 2, baseVertex + 3 });
		}

		Submesh submesh;
		submesh.Name = "Cube";
		submesh.IndexOffset = 0;
		submesh.IndexCount = static_cast<uint32_t>(source->Indices.size());
		submesh.MaterialIndex = 0;
		source->Submeshes.push_back(submesh);

		source->RecalculateBounds();
		return source;
	}

	Ref<StaticMeshResource> StaticMeshResource::CreateSphere(float radius, uint32_t segments, uint32_t rings)
	{
		Ref<StaticMeshResource> source = CreateRef<StaticMeshResource>();
		const float pi = glm::pi<float>();

		source->Vertices.reserve((rings + 1) * (segments + 1));
		source->Indices.reserve(rings * segments * 6);

		for (uint32_t ring = 0; ring <= rings; ring++)
		{
			float v = static_cast<float>(ring) / static_cast<float>(rings);
			float phi = v * pi; // 0..pi, top to bottom

			for (uint32_t seg = 0; seg <= segments; seg++)
			{
				float u = static_cast<float>(seg) / static_cast<float>(segments);
				float theta = u * 2.0f * pi; // 0..2pi

				float sinPhi = glm::sin(phi);
				glm::vec3 normal = {
					sinPhi * glm::cos(theta),
					glm::cos(phi),
					sinPhi * glm::sin(theta)
				};

				MeshVertex vertex;
				vertex.Position = normal * radius;
				vertex.Normal = normal;
				// Tangent = direction of increasing theta
				vertex.Tangent = glm::vec4(
					-glm::sin(theta), 0.0f, glm::cos(theta), 1.0f);
				vertex.TexCoord = { u, v };
				source->Vertices.push_back(vertex);
			}
		}

		for (uint32_t ring = 0; ring < rings; ring++)
		{
			for (uint32_t seg = 0; seg < segments; seg++)
			{
				uint32_t i0 = ring * (segments + 1) + seg;
				uint32_t i1 = i0 + 1;
				uint32_t i2 = i0 + (segments + 1);
				uint32_t i3 = i2 + 1;

				source->Indices.insert(source->Indices.end(), { i0, i2, i1, i1, i2, i3 });
			}
		}

		Submesh submesh;
		submesh.Name = "Sphere";
		submesh.IndexOffset = 0;
		submesh.IndexCount = static_cast<uint32_t>(source->Indices.size());
		submesh.MaterialIndex = 0;
		source->Submeshes.push_back(submesh);

		source->RecalculateBounds();
		return source;
	}

	Ref<StaticMeshResource> StaticMeshResource::CreatePlane(float size)
	{
		Ref<StaticMeshResource> source = CreateRef<StaticMeshResource>();
		const float h = size * 0.5f;

		const glm::vec3 normal = { 0.0f, 1.0f, 0.0f };
		const glm::vec4 tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
		const glm::vec3 positions[4] = {
			{ -h, 0.0f,  h }, {  h, 0.0f,  h },
			{  h, 0.0f, -h }, { -h, 0.0f, -h },
		};
		const glm::vec2 uvs[4] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };

		source->Vertices.resize(4);
		for (int i = 0; i < 4; i++)
		{
			source->Vertices[i].Position = positions[i];
			source->Vertices[i].Normal = normal;
			source->Vertices[i].Tangent = tangent;
			source->Vertices[i].TexCoord = uvs[i];
		}
		source->Indices = { 0, 1, 2, 0, 2, 3 };

		Submesh submesh;
		submesh.Name = "Plane";
		submesh.IndexOffset = 0;
		submesh.IndexCount = 6;
		submesh.MaterialIndex = 0;
		source->Submeshes.push_back(submesh);

		source->RecalculateBounds();
		return source;
	}

	Ref<StaticMeshResource> StaticMeshResource::CreateQuad()
	{
		Ref<StaticMeshResource> source = CreateRef<StaticMeshResource>();

		const glm::vec3 normal = { 0.0f, 0.0f, 1.0f };
		const glm::vec4 tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
		const glm::vec3 positions[4] = {
			{ -0.5f, -0.5f, 0.0f }, {  0.5f, -0.5f, 0.0f },
			{  0.5f,  0.5f, 0.0f }, { -0.5f,  0.5f, 0.0f },
		};
		const glm::vec2 uvs[4] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };

		source->Vertices.resize(4);
		for (int i = 0; i < 4; i++)
		{
			source->Vertices[i].Position = positions[i];
			source->Vertices[i].Normal = normal;
			source->Vertices[i].Tangent = tangent;
			source->Vertices[i].TexCoord = uvs[i];
		}
		source->Indices = { 0, 1, 2, 0, 2, 3 };

		Submesh submesh;
		submesh.Name = "Quad";
		submesh.IndexOffset = 0;
		submesh.IndexCount = 6;
		submesh.MaterialIndex = 0;
		source->Submeshes.push_back(submesh);

		source->RecalculateBounds();
		return source;
	}

} // namespace Candy
