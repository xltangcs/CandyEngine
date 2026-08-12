#pragma once

#include "Runtime/RHI/RHITypes.h"

#include <vector>

namespace Candy {

	// =========================================================================
	// RenderPassColorAttachment
	// =========================================================================
	struct RenderPassColorAttachment
	{
		RHIFormat Format   = RHIFormat::R8G8B8A8Unorm;
		LoadOp    LoadOp   = LoadOp::Clear;
		StoreOp   StoreOp  = StoreOp::Store;
		float     ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	};

	// =========================================================================
	// RenderPassDepthStencilAttachment
	// =========================================================================
	struct RenderPassDepthStencilAttachment
	{
		RHIFormat Format        = RHIFormat::D32Float;
		LoadOp    DepthLoadOp   = LoadOp::Clear;
		StoreOp   DepthStoreOp  = StoreOp::Store;
		LoadOp    StencilLoadOp  = LoadOp::DontCare;
		StoreOp   StencilStoreOp = StoreOp::DontCare;
		float     ClearDepth     = 1.0f;
		uint8_t   ClearStencil   = 0;
	};

	// =========================================================================
	// RenderPassDesc — describes the framebuffer attachments for a render pass
	// =========================================================================
	struct RenderPassDesc
	{
		std::vector<RenderPassColorAttachment>      ColorAttachments;
		RenderPassDepthStencilAttachment            DepthStencilAttachment;
		bool                                        HasDepthStencil = false;
	};

} // namespace Candy
