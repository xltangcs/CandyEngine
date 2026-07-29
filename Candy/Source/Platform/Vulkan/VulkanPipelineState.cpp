#include <Windows.h>
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Platform/Vulkan/VulkanPipelineState.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	VulkanGraphicsPipeline::VulkanGraphicsPipeline(const GraphicsPipelineDesc& desc)
		: m_Desc(desc)
	{
		CANDY_CORE_INFO("VulkanGraphicsPipeline: created (topology: {}, samples: {})",
		                static_cast<int>(desc.Topology), desc.SampleCount);
	}

	VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
	{
		CANDY_CORE_INFO("VulkanGraphicsPipeline: destroyed");
	}

} // namespace Candy
