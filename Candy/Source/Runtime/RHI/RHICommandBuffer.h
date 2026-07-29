#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/RHI/RHITypes.h"
#include "Runtime/RHI/RHIRenderPass.h"

#include <cstdint>

namespace Candy {

	// Forward declarations
	class RHIBuffer;
	class RHITexture;
	class RHISampler;
	class RHIGraphicsPipeline;

	// =========================================================================
	// RHICommandBuffer — records GPU commands for later submission
	// =========================================================================
	class RHICommandBuffer
	{
	public:
		virtual ~RHICommandBuffer() = default;

		// ---- Lifetime ------------------------------------------------------

		virtual void Begin() = 0;
		virtual void End()   = 0;

		// ---- Render pass ---------------------------------------------------

		virtual void BeginRenderPass(const RenderPassDesc& desc) = 0;
		virtual void EndRenderPass() = 0;

		// ---- Pipeline & state ----------------------------------------------

		virtual void SetPipeline(const Ref<RHIGraphicsPipeline>& pipeline) = 0;

		virtual void SetViewport(float x, float y, float width, float height,
		                         float minDepth = 0.0f, float maxDepth = 1.0f) = 0;

		virtual void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;

		// ---- Resource binding (slot-based, like DX12 root params) ----------

		virtual void SetVertexBuffer(const Ref<RHIBuffer>& buffer, uint32_t slot = 0, uint64_t offset = 0) = 0;
		virtual void SetIndexBuffer(const Ref<RHIBuffer>& buffer, IndexFormat format = IndexFormat::UInt32, uint64_t offset = 0) = 0;

		virtual void SetConstantBuffer(uint32_t slot, uint32_t binding, const Ref<RHIBuffer>& buffer) = 0;
		virtual void SetTexture(uint32_t slot, uint32_t binding, const Ref<RHITexture>& texture) = 0;
		virtual void SetSampler(uint32_t slot, uint32_t binding, const Ref<RHISampler>& sampler) = 0;

		// ---- Draw calls ----------------------------------------------------

		virtual void Draw(uint32_t vertexCount,
		                  uint32_t instanceCount = 1,
		                  uint32_t firstVertex   = 0,
		                  uint32_t firstInstance = 0) = 0;

		virtual void DrawIndexed(uint32_t indexCount,
		                         uint32_t instanceCount = 1,
		                         uint32_t firstIndex    = 0,
		                         int32_t  vertexOffset  = 0,
		                         uint32_t firstInstance = 0) = 0;
	};

} // namespace Candy
