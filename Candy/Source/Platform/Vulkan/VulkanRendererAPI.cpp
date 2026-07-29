#include "Platform/Vulkan/VulkanRendererAPI.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Runtime/RHI/RHICommandBuffer.h"
#include "Runtime/RHI/RHICommandQueue.h"
#include "Runtime/RHI/RHISwapChain.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	void VulkanRendererAPI::Init(const PipelineStateDescription& defaultState)
	{
		CANDY_CORE_INFO("VulkanRendererAPI: initialized");
	}

	void VulkanRendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		m_VpX = x; m_VpY = y; m_VpWidth = width; m_VpHeight = height;

		if (m_CmdBuffer)
			m_CmdBuffer->SetViewport(static_cast<float>(x), static_cast<float>(y),
			                         static_cast<float>(width), static_cast<float>(height));
	}

	void VulkanRendererAPI::SetClearColor(const glm::vec4& color)
	{
		m_ClearColor = color;
	}

	void VulkanRendererAPI::Clear()
	{
		CANDY_CORE_WARN("TODO: VulkanRendererAPI::Clear — use BeginRenderPass instead");
	}

	void VulkanRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray,
	                                    PrimitiveTopology topology, uint32_t indexCount)
	{
		CANDY_CORE_WARN("TODO: VulkanRendererAPI::DrawIndexed — use RHI Command Buffer");
	}

	void VulkanRendererAPI::Draw(const Ref<VertexArray>& vertexArray,
	                             PrimitiveTopology topology, uint32_t elementCount)
	{
		CANDY_CORE_WARN("TODO: VulkanRendererAPI::Draw — use RHI Command Buffer");
	}

	void VulkanRendererAPI::SetLineWidth(float width)
	{
		// Vulkan requires wideLines feature for line widths > 1
	}

	void VulkanRendererAPI::SetDefaultPipelineState(const PipelineStateDescription& state)
	{
		// No global state in Vulkan
	}

} // namespace Candy
