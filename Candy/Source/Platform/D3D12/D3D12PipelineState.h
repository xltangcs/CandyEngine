#pragma once

#include "Runtime/RHI/RHIPipelineState.h"

#include <d3d12.h>
#include <wrl/client.h>

namespace Candy {

	// =========================================================================
	// D3D12GraphicsPipeline — wraps ID3D12PipelineState + ID3D12RootSignature
	// =========================================================================
	class D3D12GraphicsPipeline : public RHIGraphicsPipeline
	{
	public:
		D3D12GraphicsPipeline(const GraphicsPipelineDesc& desc);
		virtual ~D3D12GraphicsPipeline();

		[[nodiscard]] const GraphicsPipelineDesc& GetDesc() const { return m_Desc; }

		// ---- Set by D3D12Device after creation ------------------------------

		void SetNativePipeline(Microsoft::WRL::ComPtr<ID3D12PipelineState> pso,
		                       Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig);

		[[nodiscard]] ID3D12PipelineState*  GetNativePipelineState() const { return m_PSO.Get(); }
		[[nodiscard]] ID3D12RootSignature*  GetRootSignature()       const { return m_RootSignature.Get(); }

	private:
		GraphicsPipelineDesc m_Desc;

		Microsoft::WRL::ComPtr<ID3D12PipelineState>   m_PSO;
		Microsoft::WRL::ComPtr<ID3D12RootSignature>   m_RootSignature;
	};

} // namespace Candy
