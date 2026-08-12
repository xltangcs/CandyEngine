#include "CandyPCH.h"
#include <Windows.h>
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Platform/Vulkan/VulkanPipelineState.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	VulkanGraphicsPipeline::VulkanGraphicsPipeline(const GraphicsPipelineDesc& desc) : m_Desc(desc) {}
	VulkanGraphicsPipeline::~VulkanGraphicsPipeline() = default;

	void VulkanGraphicsPipeline::SetVkPipeline(VkPipeline pipeline, VkPipelineLayout layout)
	{
		m_Pipeline       = pipeline;
		m_PipelineLayout  = layout;
	}

} // namespace Candy
