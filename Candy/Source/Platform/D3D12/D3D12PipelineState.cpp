#include "CandyPCH.h"
#include <Windows.h>
#include <d3d12.h>

#include "Platform/D3D12/D3D12PipelineState.h"
#include "Runtime/Core/Log.h"

using Microsoft::WRL::ComPtr;

namespace Candy {

	D3D12GraphicsPipeline::D3D12GraphicsPipeline(const GraphicsPipelineDesc& desc)
		: m_Desc(desc)
	{
		CANDY_CORE_INFO("D3D12GraphicsPipeline: created (topology: {}, samples: {})",
		                static_cast<int>(desc.Topology), desc.SampleCount);
	}

	D3D12GraphicsPipeline::~D3D12GraphicsPipeline()
	{
		CANDY_CORE_INFO("D3D12GraphicsPipeline: destroyed");
	}

	void D3D12GraphicsPipeline::SetNativePipeline(ComPtr<ID3D12PipelineState> pso,
	                                             ComPtr<ID3D12RootSignature> rootSig)
	{
		m_PSO          = std::move(pso);
		m_RootSignature = std::move(rootSig);
	}

} // namespace Candy
