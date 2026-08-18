#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/Asset/ShaderReflector.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Candy {

	// =========================================================================
	// Material — CPU-side material asset (.mat)
	//
	// Pure data container describing surface appearance, driven entirely by a
	// bound shader: all editable properties come from the shader's `//@param`
	// reflection (ShaderReflector) and are stored as name -> value overrides.
	// No shader/PSO dependency here — GPU state (pipeline, descriptor sets) is
	// produced from this data by the renderer at a later stage. Serialized
	// to/from YAML (.mat) files via the VFS.
	// =========================================================================

	class Material
	{
	public:
		Material() = default;
		~Material() = default;

		// --- Identity -----------------------------------------------------------
		std::string Name;                    // material asset name (used for .mat lookup)

		// --- Shader binding -------------------------------------------------------
		// VFS:// path to a user shader (.hlsl / .glsl). Empty = no shader bound;
		// with no shader there are no editable parameters.
		std::string ShaderPath;

		// Shader parameter values (name -> value). Parameters absent from this
		// map use the defaults declared in the shader source.
		std::unordered_map<std::string, ShaderParamValue> ShaderParams;

		// Resolves the effective parameter list for the bound shader: reflected
		// defaults merged with the overrides in ShaderParams. Returns false when
		// no shader is bound or reflection fails.
		bool GetShaderParameters(std::vector<ShaderParameter>& outParameters) const;

		// --- Serialization (.mat / YAML) ----------------------------------------
		// Writes to the given VFS:// path. Returns false on failure.
		bool Serialize(const std::string& virtualPath) const;
		// Loads from the given VFS:// path. Returns nullptr on failure.
		static Ref<Material> Load(const std::string& virtualPath);
	};

} // namespace Candy
