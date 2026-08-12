#pragma once

#include "Runtime/RHI/RHIPipelineState.h"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

namespace Candy {

	class VulkanGraphicsPipeline : public RHIGraphicsPipeline
	{
	public:
		VulkanGraphicsPipeline(const GraphicsPipelineDesc& desc);
		virtual ~VulkanGraphicsPipeline();

		[[nodiscard]] const GraphicsPipelineDesc& GetDesc() const { return m_Desc; }
		[[nodiscard]] VkPipeline       GetVkPipeline()       const { return m_Pipeline; }
		[[nodiscard]] VkPipelineLayout GetVkPipelineLayout() const { return m_PipelineLayout; }

		void SetVkPipeline(VkPipeline pipeline, VkPipelineLayout layout);

	private:
		GraphicsPipelineDesc m_Desc;
		VkPipeline       m_Pipeline       = VK_NULL_HANDLE;
		VkPipelineLayout m_PipelineLayout  = VK_NULL_HANDLE;
	};

} // namespace Candy
