#pragma once

#include <cstdint>

namespace Candy {

	// =========================================================================
	// Shader stage
	// =========================================================================
	enum class ShaderStage
	{
		None = 0,
		Vertex,
		Fragment,
		Geometry,
		Compute
	};

	// =========================================================================
	// Shader language
	// =========================================================================
	enum class ShaderLanguage
	{
		None = 0,
		GLSL,
		HLSL,
		SPIRV
	};

	// =========================================================================
	// Primitive topology
	// =========================================================================
	enum class PrimitiveTopology
	{
		None = 0,
		Triangles,
		Lines,
		Points
	};

	// =========================================================================
	// Attachment format
	// =========================================================================
	enum class AttachmentFormat
	{
		None = 0,
		RGBA8,
		RGBA32F,
		RedInteger,
		Depth24Stencil8,
		Depth32F
	};

	// =========================================================================
	// Platform-agnostic resource handle
	// =========================================================================
	struct RHIHandle
	{
		uint32_t Value = 0;

		explicit operator bool() const { return Value != 0; }
		bool operator==(const RHIHandle& other) const { return Value == other.Value; }
		bool operator!=(const RHIHandle& other) const { return Value != other.Value; }
	};

	// =========================================================================
	// Platform-agnostic window handle
	// =========================================================================
	struct WindowHandle
	{
		void* Native = nullptr;
	};

} // namespace Candy
