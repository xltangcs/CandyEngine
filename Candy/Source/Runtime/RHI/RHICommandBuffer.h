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
	class RHIFramebuffer;
	class RHIComputePipeline;

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

		/// Begin a render pass targeting `target`; nullptr targets the swap chain
		/// back buffer (resolved from RHIContext). The target's attachments are
		/// bound and cleared per-attachment LoadOp before any draw.
		virtual void BeginRenderPass(RHIFramebuffer* target, const RenderPassDesc& desc) = 0;
		virtual void EndRenderPass() = 0;

		// ---- Pipeline & state ----------------------------------------------

		virtual void SetPipeline(const Ref<RHIGraphicsPipeline>& pipeline) = 0;

		virtual void SetViewport(float x, float y, float width, float height,
		                         float minDepth = 0.0f, float maxDepth = 1.0f) = 0;

		virtual void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;

		// ---- Resource binding (slot-based, like D3D12 root params) ----------

		virtual void SetVertexBuffer(const Ref<RHIBuffer>& buffer, uint32_t slot = 0, uint64_t offset = 0) = 0;
		virtual void SetIndexBuffer(const Ref<RHIBuffer>& buffer, IndexFormat format = IndexFormat::UInt32, uint64_t offset = 0) = 0;

		/// Binds a constant buffer (or a 256B-aligned slice at `offset` within it)
		/// to root parameter `slot`. Per-draw slices let a single frame-scoped
		/// buffer hold per-draw data without GPU-side aliasing between draws.
		virtual void SetConstantBuffer(uint32_t slot, uint32_t binding, const Ref<RHIBuffer>& buffer, uint64_t offset = 0) = 0;
		/// Binds `count` textures as one contiguous SRV descriptor table rooted at
		/// root parameter `slot` (bindings 0..count-1). The backend allocates a
		/// fresh descriptor range per call, so consecutive draws never alias each
		/// other's texture bindings within a command list.
		virtual void SetTextures(uint32_t slot, uint32_t count, const Ref<RHITexture>* textures) = 0;
		virtual void SetSampler(uint32_t slot, uint32_t binding, const Ref<RHISampler>& sampler) = 0;

		// ---- Compute (D3D12-only; other backends inherit no-op defaults) ----

		virtual void SetComputePipeline(const Ref<RHIComputePipeline>& pipeline) { (void)pipeline; }
		/// Binds a constant buffer to compute root parameter `slot` (CBV b0..).
		virtual void SetComputeConstantBuffer(uint32_t slot, const Ref<RHIBuffer>& buffer, uint64_t offset = 0)
		{
			(void)slot; (void)buffer; (void)offset;
		}
		/// Binds `count` textures as an SRV table at compute root parameter `slot`.
		virtual void SetComputeTextures(uint32_t slot, uint32_t count, const Ref<RHITexture>* textures)
		{
			(void)slot; (void)count; (void)textures;
		}
		/// Binds `count` textures as a UAV table at compute root parameter `slot`.
		/// `mipSlice` selects the mip level of each UAV view (cubemap bakes
		/// write one mip per dispatch).
		virtual void SetComputeUAVs(uint32_t slot, uint32_t count, const Ref<RHITexture>* textures,
		                            uint32_t mipSlice = 0)
		{
			(void)slot; (void)count; (void)textures; (void)mipSlice;
		}
		/// Dispatch `groupCountX/Y/Z` thread groups.
		virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
		{
			(void)groupCountX; (void)groupCountY; (void)groupCountZ;
		}
		/// Insert a UAV barrier so previous dispatches' writes are visible to
		/// subsequent reads/writes of the same resource.
		virtual void UAVBarrier() {}

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
