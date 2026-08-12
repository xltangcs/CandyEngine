#pragma once

#include "Runtime/RHI/RHITypes.h"

#include <vector>

namespace Candy {

	// =========================================================================
	// VertexInputLayout — describes per-vertex attribute layout
	// =========================================================================
	struct VertexInputLayout
	{
		/// A single vertex attribute (e.g. position, normal, uv)
		struct VertexAttribute
		{
			uint32_t Location  = 0;   ///< shader input location
			uint32_t Binding   = 0;   ///< which vertex buffer binding
			RHIFormat Format   = RHIFormat::Unknown;
			uint32_t Offset    = 0;   ///< byte offset within the stride
		};

		/// Describes a vertex buffer binding slot stride and rate
		struct VertexBinding
		{
			uint32_t Binding     = 0;
			uint32_t Stride      = 0;   ///< bytes between consecutive vertices
			bool     PerInstance = false;
		};

		std::vector<VertexAttribute> Attributes;
		std::vector<VertexBinding>   Bindings;
	};

	// =========================================================================
	// RasterizerState
	// =========================================================================
	struct RasterizerState
	{
		CullMode Cull                = CullMode::Back;
		FillMode Fill                = FillMode::Solid;
		bool     DepthClipEnable     = true;
		int32_t  DepthBias           = 0;
		float    DepthBiasSlopeFactor = 0.0f;
		float    DepthBiasClamp      = 0.0f;
	};

	// =========================================================================
	// DepthStencilState — depth testing; stencil omitted for now
	// =========================================================================
	struct DepthStencilState
	{
		bool      DepthTestEnable  = true;
		bool      DepthWriteEnable = true;
		CompareOp DepthCompareOp   = CompareOp::Less;
	};

	// =========================================================================
	// BlendState
	// =========================================================================
	struct BlendState
	{
		bool           BlendEnable = true;
		ColorWriteMask WriteMask   = ColorWriteMask::All;

		// Blend factors — using canonical D3D/Vulkan naming
		enum class BlendFactor
		{
			Zero,
			One,
			SrcColor,
			OneMinusSrcColor,
			DstColor,
			OneMinusDstColor,
			SrcAlpha,
			OneMinusSrcAlpha,
			DstAlpha,
			OneMinusDstAlpha,
			SrcAlphaSaturate
		};

		enum class BlendOp
		{
			Add,
			Subtract,
			ReverseSubtract,
			Min,
			Max
		};

		BlendFactor SrcColorBlendFactor = BlendFactor::SrcAlpha;
		BlendFactor DstColorBlendFactor = BlendFactor::OneMinusSrcAlpha;
		BlendOp     ColorBlendOp        = BlendOp::Add;
		BlendFactor SrcAlphaBlendFactor = BlendFactor::One;
		BlendFactor DstAlphaBlendFactor = BlendFactor::Zero;
		BlendOp     AlphaBlendOp        = BlendOp::Add;
	};

	// =========================================================================
	// GraphicsPipelineDesc — full immutable pipeline state
	// =========================================================================
	struct GraphicsPipelineDesc
	{
		VertexInputLayout       VertexInput;
		RasterizerState         Rasterizer;
		DepthStencilState       DepthStencil;
		BlendState              Blend;
		PrimitiveTopology       Topology        = PrimitiveTopology::Triangles;
		std::vector<RHIFormat>  RenderTargetFormats;
		RHIFormat               DepthStencilFormat = RHIFormat::Unknown;
		uint32_t                SampleCount     = 1;
	};

	// =========================================================================
	// RHIGraphicsPipeline — opaque graphics pipeline state object
	// =========================================================================
	class RHIGraphicsPipeline
	{
	public:
		virtual ~RHIGraphicsPipeline() = default;
	};

} // namespace Candy
