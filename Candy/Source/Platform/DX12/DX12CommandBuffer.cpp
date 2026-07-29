#include <Windows.h>
#include <d3d12.h>

#include "Platform/DX12/DX12CommandBuffer.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	DX12CommandBuffer::DX12CommandBuffer()
	{
	}

	DX12CommandBuffer::~DX12CommandBuffer()
	{
	}

	// ---- Lifetime ------------------------------------------------------------

	void DX12CommandBuffer::Begin()
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::Begin — not yet implemented");
	}

	void DX12CommandBuffer::End()
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::End — not yet implemented");
	}

	// ---- Render pass ---------------------------------------------------------

	void DX12CommandBuffer::BeginRenderPass(const RenderPassDesc& desc)
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::BeginRenderPass — not yet implemented");
	}

	void DX12CommandBuffer::EndRenderPass()
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::EndRenderPass — not yet implemented");
	}

	// ---- Pipeline & state ----------------------------------------------------

	void DX12CommandBuffer::SetPipeline(const Ref<RHIGraphicsPipeline>& pipeline)
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::SetPipeline — not yet implemented");
	}

	void DX12CommandBuffer::SetViewport(float x, float y, float width, float height,
	                                    float minDepth, float maxDepth)
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::SetViewport — not yet implemented");
	}

	void DX12CommandBuffer::SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height)
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::SetScissor — not yet implemented");
	}

	// ---- Resource binding ----------------------------------------------------

	void DX12CommandBuffer::SetVertexBuffer(const Ref<RHIBuffer>& buffer, uint32_t slot, uint64_t offset)
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::SetVertexBuffer — not yet implemented");
	}

	void DX12CommandBuffer::SetIndexBuffer(const Ref<RHIBuffer>& buffer, IndexFormat format, uint64_t offset)
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::SetIndexBuffer — not yet implemented");
	}

	void DX12CommandBuffer::SetConstantBuffer(uint32_t slot, uint32_t binding, const Ref<RHIBuffer>& buffer)
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::SetConstantBuffer — not yet implemented");
	}

	void DX12CommandBuffer::SetTexture(uint32_t slot, uint32_t binding, const Ref<RHITexture>& texture)
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::SetTexture — not yet implemented");
	}

	void DX12CommandBuffer::SetSampler(uint32_t slot, uint32_t binding, const Ref<RHISampler>& sampler)
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::SetSampler — not yet implemented");
	}

	// ---- Draw calls ----------------------------------------------------------

	void DX12CommandBuffer::Draw(uint32_t vertexCount,
	                             uint32_t instanceCount,
	                             uint32_t firstVertex,
	                             uint32_t firstInstance)
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::Draw — not yet implemented");
	}

	void DX12CommandBuffer::DrawIndexed(uint32_t indexCount,
	                                    uint32_t instanceCount,
	                                    uint32_t firstIndex,
	                                    int32_t  vertexOffset,
	                                    uint32_t firstInstance)
	{
		CANDY_CORE_WARN("TODO: DX12CommandBuffer::DrawIndexed — not yet implemented");
	}

} // namespace Candy
