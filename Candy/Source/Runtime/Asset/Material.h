#pragma once

#include "Runtime/Core/Base.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>

namespace Candy {

	// =========================================================================
	// Material — CPU-side material asset (.mat)
	//
	// Pure data container describing surface appearance. No shader/PSO
	// dependency here — GPU state (pipeline, descriptor sets) is produced from
	// this data by the renderer at a later stage. Serialized to/from YAML
	// (.mat) files via the VFS.
	// =========================================================================

	enum class MaterialBlendMode
	{
		Opaque = 0,
		Masked,
		Transparent,

		Count
	};

	enum class MaterialShadingModel
	{
		Lit = 0,
		Unlit,

		Count
	};

	class Material
	{
	public:
		Material() = default;
		~Material() = default;

		// --- Identity -----------------------------------------------------------
		std::string Name;                    // material asset name (used for .mat lookup)

		// --- Surface properties -------------------------------------------------
		glm::vec3 BaseColor = glm::vec3(1.0f);
		float     Metallic  = 0.0f;
		float     Roughness = 0.5f;

		glm::vec3 EmissiveColor     = glm::vec3(0.0f);
		float     EmissiveIntensity = 0.0f;

		MaterialBlendMode    BlendMode    = MaterialBlendMode::Opaque;
		MaterialShadingModel ShadingModel = MaterialShadingModel::Lit;

		// --- Texture maps (VFS:// paths, empty = none) --------------------------
		std::string BaseColorMap;          // albedo / base color
		std::string NormalMap;             // tangent-space normal
		std::string MetallicRoughnessMap;  // G = metallic, B = roughness
		std::string EmissiveMap;           // emissive color

		glm::vec2 UVTiling = glm::vec2(1.0f);
		glm::vec2 UVOffset = glm::vec2(0.0f);

		// --- Serialization (.mat / YAML) ----------------------------------------
		// Writes to the given VFS:// path. Returns false on failure.
		bool Serialize(const std::string& virtualPath) const;
		// Loads from the given VFS:// path. Returns nullptr on failure.
		static Ref<Material> Load(const std::string& virtualPath);

		// Helpers
		bool HasBaseColorMap()          const { return !BaseColorMap.empty(); }
		bool HasNormalMap()             const { return !NormalMap.empty(); }
		bool HasMetallicRoughnessMap()  const { return !MetallicRoughnessMap.empty(); }
		bool HasEmissiveMap()           const { return !EmissiveMap.empty(); }
	};

} // namespace Candy
