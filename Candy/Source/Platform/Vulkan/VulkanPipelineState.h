#pragma once

#include "Runtime/RHI/RHIPipelineState.h"

namespace Candy {

	// =========================================================================
	// VulkanGraphicsPipeline — Vulkan pipeline state object skeleton
	//
	// Wraps VkPipeline once the Vulkan SDK is integrated.
	// =========================================================================
	class VulkanGraphicsPipeline : public RHIGraphicsPipeline
	{
	public:
		VulkanGraphicsPipeline(const GraphicsPipelineDesc& desc);
		virtual ~VulkanGraphicsPipeline();

		[[nodiscard]] const GraphicsPipelineDesc& GetDesc() const { return m_Desc; }

	private:
		GraphicsPipelineDesc m_Desc;
	};

} // namespace Candy
