#include "CandyPCH.h"
#include <Windows.h>
#include <d3d12.h>

#include "Platform/DX12/DX12PipelineState.h"
#include "Runtime/Core/Log.h"

using Microsoft::WRL::ComPtr;

namespace Candy {

	DX12GraphicsPipeline::DX12GraphicsPipeline(const GraphicsPipelineDesc& desc)
		: m_Desc(desc)
	{
		CANDY_CORE_INFO("DX12GraphicsPipeline: created (topology: {}, samples: {})",
		                static_cast<int>(desc.Topology), desc.SampleCount);
	}

	DX12GraphicsPipeline::~DX12GraphicsPipeline()
	{
		CANDY_CORE_INFO("DX12GraphicsPipeline: destroyed");
	}

	void DX12GraphicsPipeline::SetNativePipeline(ComPtr<ID3D12PipelineState> pso,
	                                             ComPtr<ID3D12RootSignature> rootSig)
	{
		m_PSO          = std::move(pso);
		m_RootSignature = std::move(rootSig);
	}

} // namespace Candy
