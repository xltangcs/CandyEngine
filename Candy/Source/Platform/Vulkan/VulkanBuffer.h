#pragma once

#include "Runtime/RHI/RHIDevice.h"
#include <vulkan/vulkan.h>

namespace Candy {

	class VulkanDevice;

	// =========================================================================
	// VulkanBuffer — VkBuffer + VkDeviceMemory wrapper
	// =========================================================================
	class VulkanBuffer : public RHIBuffer
	{
	public:
		VulkanBuffer(VulkanDevice* device, const BufferDesc& desc);
		virtual ~VulkanBuffer();

		const BufferDesc& GetDesc() const override;
		void* Map() override;
		void  Unmap() override;

		[[nodiscard]] VkBuffer GetVkBuffer() const { return m_Buffer; }

	private:
		void Create(VulkanDevice* device);
		uint32_t FindMemoryType(VulkanDevice* device, uint32_t typeFilter, VkMemoryPropertyFlags props);

		BufferDesc  m_Desc;
		VkBuffer    m_Buffer     = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory  = VK_NULL_HANDLE;
		void*       m_MappedData = nullptr;
		VulkanDevice* m_DevicePtr = nullptr;
	};

} // namespace Candy
