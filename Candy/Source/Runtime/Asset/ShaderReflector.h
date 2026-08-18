#pragma once

#include "Runtime/Core/Base.h"

#include <glm/glm.hpp>

#include <string>
#include <variant>
#include <vector>

namespace Candy {

	// =========================================================================
	// ShaderReflector --- lightweight source-level parameter reflection.
	//
	// Reflects editable material parameters out of shader source (HLSL / GLSL)
	// using the `//@param` convention, mirroring how Godot exposes shader
	// uniforms in the inspector --- the editor-facing metadata (type, hint,
	// default) is declared in source, parsed at load time, and drives the
	// property widgets. No GPU / bytecode reflection involved.
	//
	//   cbuffer MaterialCB : register(b1)
	//   {
	//       float4 u_BaseColor;   // @param color "Base Color" = (1.0, 1.0, 1.0, 1.0)
	//       float  u_Metallic;    // @param range(0, 1, 0.01) "Metallic" = 0.5
	//       float  u_Mode;        // @param enum(Opaque, Masked, Transparent) "Mode" = Opaque
	//       int    u_MaxLights;   // @param range(0, 8, 1) "Max Lights" = 4
	//   };
	//   Texture2D u_AlbedoMap : register(t0);  // @param texture "Albedo Map"
	//
	// Declarations without a `//@param` marker are ignored, so existing engine
	// shaders are unaffected.
	// =========================================================================

	enum class ShaderParamType
	{
		Float = 0,
		Int,
		Bool,
		Vec2,
		Vec3,
		Vec4,
		Color,
		Texture,

		Count
	};

	struct ShaderParamRange
	{
		float Min  = 0.0f;
		float Max  = 1.0f;
		float Step = 0.01f;
	};

	// Runtime value of a single material parameter. For enums the value is an
	// Int (index into EnumLabels); for textures it is a VFS:// path string.
	using ShaderParamValue = std::variant<float, int, bool, glm::vec2, glm::vec3, glm::vec4, std::string>;

	struct ShaderParameter
	{
		std::string Name;                    // shader symbol, e.g. "u_BaseColor"
		std::string DisplayName;             // inspector label, e.g. "Base Color"
		ShaderParamType Type = ShaderParamType::Float;

		bool IsEnum = false;                 // @param enum(A, B, C) --- value = Int index
		std::vector<std::string> EnumLabels;

		bool HasRange = false;               // @param range(min, max, step)
		ShaderParamRange Range;

		ShaderParamValue Default;            // declared default (or current value when overridden)
	};

	class ShaderReflector
	{
	public:
		/// Parses shader source and returns the reflected parameter list.
		/// Returns an empty vector when the source has no @param markers.
		static std::vector<ShaderParameter> Reflect(const std::string& source);
	};

} // namespace Candy
