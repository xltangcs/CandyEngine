#include <Windows.h>
#include <d3d12.h>

#include "Platform/DX12/DX12PipelineState.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	DX12GraphicsPipeline::DX12GraphicsPipeline(const GraphicsPipelineDesc& desc)
		: m_Desc(desc)
	{
		CANDY_CORE_INFO("DX12GraphicsPipeline: created (topology: {})",
		                static_cast<int>(desc.Topology));
	}

	DX12GraphicsPipeline::~DX12GraphicsPipeline()
	{
		CANDY_CORE_INFO("DX12GraphicsPipeline: destroyed");
	}

} // namespace Candy
