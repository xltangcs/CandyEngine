#pragma once

#include "Runtime/RHI/RHIPipelineState.h"

namespace Candy {

	// =========================================================================
	// DX12GraphicsPipeline — Direct3D 12 pipeline state object skeleton
	//
	// Wraps ID3D12PipelineState + ID3D12RootSignature once the DX12 Agility
	// SDK is integrated.
	// =========================================================================
	class DX12GraphicsPipeline : public RHIGraphicsPipeline
	{
	public:
		DX12GraphicsPipeline(const GraphicsPipelineDesc& desc);
		virtual ~DX12GraphicsPipeline();

		[[nodiscard]] const GraphicsPipelineDesc& GetDesc() const { return m_Desc; }

	private:
		GraphicsPipelineDesc m_Desc;
	};

} // namespace Candy
