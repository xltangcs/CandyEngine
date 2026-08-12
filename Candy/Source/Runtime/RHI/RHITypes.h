#pragma once

#include <cstdint>
#include <type_traits>

// Macro to enable bitwise operations on enum class flags
#define CANDY_DEFINE_ENUM_FLAG_OPERATORS(Enum)                                      \
	inline constexpr Enum  operator|(Enum a, Enum b)                                \
	{                                                                               \
		return static_cast<Enum>(                                                   \
			static_cast<std::underlying_type_t<Enum>>(a)                            \
			| static_cast<std::underlying_type_t<Enum>>(b));                        \
	}                                                                               \
	inline constexpr Enum  operator&(Enum a, Enum b)                                \
	{                                                                               \
		return static_cast<Enum>(                                                   \
			static_cast<std::underlying_type_t<Enum>>(a)                            \
			& static_cast<std::underlying_type_t<Enum>>(b));                        \
	}                                                                               \
	inline constexpr Enum& operator|=(Enum& a, Enum b) { return a = a | b; }        \
	inline constexpr Enum& operator&=(Enum& a, Enum b) { return a = a & b; }        \
	inline constexpr bool  HasFlag(Enum a, Enum b)                                  \
	{                                                                               \
		return (static_cast<std::underlying_type_t<Enum>>(a)                        \
				& static_cast<std::underlying_type_t<Enum>>(b))                     \
			!= 0;                                                                   \
	}

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
		TessellationControl,
		TessellationEvaluation,
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
		Points,
		TriangleStrip,
		LineStrip
	};

	// =========================================================================
	// Attachment format (for Framebuffer specs)
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
	// Unified RHI format — covers textures, vertex attributes and depth/stencil
	// =========================================================================
	enum class RHIFormat : uint32_t
	{
		Unknown = 0,

		// --- 8-bit / channel -------------------------------------------------
		R8Unorm,
		R8Snorm,
		R8Uint,
		R8Sint,

		// --- 16-bit / channel ------------------------------------------------
		R16Unorm,
		R16Snorm,
		R16Uint,
		R16Sint,
		R16Float,
		R8G8Unorm,
		R8G8Snorm,
		R8G8Uint,
		R8G8Sint,

		// --- 32-bit / channel ------------------------------------------------
		R32Uint,
		R32Sint,
		R32Float,
		R8G8B8A8Unorm,
		R8G8B8A8Snorm,
		R8G8B8A8Uint,
		R8G8B8A8Sint,
		R8G8B8A8Srgb,
		R16G16Unorm,
		R16G16Snorm,
		R16G16Uint,
		R16G16Sint,
		R16G16Float,
		B8G8R8A8Unorm,
		B8G8R8A8Srgb,

		// --- 64-bit / channel ------------------------------------------------
		R32G32Uint,
		R32G32Sint,
		R32G32Float,
		R16G16B16A16Unorm,
		R16G16B16A16Snorm,
		R16G16B16A16Uint,
		R16G16B16A16Sint,
		R16G16B16A16Float,

		// --- 96-bit / channel ------------------------------------------------
		R32G32B32Uint,
		R32G32B32Sint,
		R32G32B32Float,

		// --- 128-bit / channel -----------------------------------------------
		R32G32B32A32Uint,
		R32G32B32A32Sint,
		R32G32B32A32Float,

		// --- Depth / Stencil -------------------------------------------------
		D16Unorm,
		D32Float,
		D24UnormS8Uint,
		D32FloatS8Uint,
	};

	// =========================================================================
	// Index format
	// =========================================================================
	enum class IndexFormat
	{
		UInt16,
		UInt32
	};

	// =========================================================================
	// Texture filtering
	// =========================================================================
	enum class SamplerFilter
	{
		Nearest,
		Linear
	};

	// =========================================================================
	// Texture addressing / wrap mode
	// =========================================================================
	enum class SamplerAddressMode
	{
		Repeat,
		MirroredRepeat,
		ClampToEdge,
		ClampToBorder
	};

	// =========================================================================
	// Depth / Stencil comparison function
	// =========================================================================
	enum class CompareOp
	{
		Never,
		Less,
		Equal,
		LessEqual,
		Greater,
		NotEqual,
		GreaterEqual,
		Always
	};

	// =========================================================================
	// Attachment load operation
	// =========================================================================
	enum class LoadOp
	{
		Load,
		Clear,
		DontCare
	};

	// =========================================================================
	// Attachment store operation
	// =========================================================================
	enum class StoreOp
	{
		Store,
		DontCare
	};

	// =========================================================================
	// Buffer / Texture usage flags (bitmask)
	// =========================================================================
	enum class ResourceUsage : uint32_t
	{
		None           = 0,
		VertexBuffer   = 1 << 0,
		IndexBuffer    = 1 << 1,
		ConstantBuffer = 1 << 2,
		ShaderRead     = 1 << 3,  ///< SRV / sampled image
		ShaderWrite    = 1 << 4,  ///< UAV / storage image
		RenderTarget   = 1 << 5,  ///< RTV / color attachment
		DepthStencil   = 1 << 6,  ///< DSV / depth-stencil attachment
		CopySrc        = 1 << 7,
		CopyDst        = 1 << 8,
		Indirect       = 1 << 9
	};
	CANDY_DEFINE_ENUM_FLAG_OPERATORS(ResourceUsage)

	// =========================================================================
	// Face culling mode
	// =========================================================================
	enum class CullMode
	{
		None,
		Front,
		Back
	};

	// =========================================================================
	// Polygon fill mode
	// =========================================================================
	enum class FillMode
	{
		Solid,
		Wireframe
	};

	// =========================================================================
	// Color write mask (bitmask)
	// =========================================================================
	enum class ColorWriteMask : uint8_t
	{
		None  = 0,
		Red   = 1 << 0,
		Green = 1 << 1,
		Blue  = 1 << 2,
		Alpha = 1 << 3,
		All   = Red | Green | Blue | Alpha
	};
	CANDY_DEFINE_ENUM_FLAG_OPERATORS(ColorWriteMask)

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
