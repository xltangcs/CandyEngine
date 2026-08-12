#include "CandyPCH.h"
#include <Windows.h>
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Platform/Vulkan/VulkanBuffer.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	VulkanBuffer::VulkanBuffer(VulkanDevice* device, const BufferDesc& desc)
		: m_Desc(desc), m_DevicePtr(device)
	{
		Create(device);
	}

	VulkanBuffer::~VulkanBuffer()
	{
		if (m_MappedData)
			Unmap();

		auto* dev = m_DevicePtr;
		if (m_Buffer && dev)
			dev->fnDestroyBuffer(dev->GetVkDevice(), m_Buffer, nullptr);
		if (m_Memory && dev)
			dev->fnFreeMemory(dev->GetVkDevice(), m_Memory, nullptr);
	}

	const BufferDesc& VulkanBuffer::GetDesc() const { return m_Desc; }

	void* VulkanBuffer::Map()
	{
		if (!m_MappedData && m_DevicePtr)
		{
			VkResult res = m_DevicePtr->fnMapMemory(
				m_DevicePtr->GetVkDevice(), m_Memory, 0, m_Desc.Size, 0, &m_MappedData);
			if (res != VK_SUCCESS)
				CANDY_CORE_ERROR("VulkanBuffer::Map failed for '{}'", m_Desc.DebugName);
		}
		return m_MappedData;
	}

	void VulkanBuffer::Unmap()
	{
		if (m_MappedData && m_DevicePtr)
		{
			m_DevicePtr->fnUnmapMemory(m_DevicePtr->GetVkDevice(), m_Memory);
			m_MappedData = nullptr;
		}
	}

	void VulkanBuffer::Create(VulkanDevice* device)
	{
		VkBufferCreateInfo bufferCI = {};
		bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCI.size  = m_Desc.Size;

		if (HasFlag(m_Desc.Usage, ResourceUsage::VertexBuffer))
			bufferCI.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		if (HasFlag(m_Desc.Usage, ResourceUsage::IndexBuffer))
			bufferCI.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		if (HasFlag(m_Desc.Usage, ResourceUsage::ConstantBuffer))
			bufferCI.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		if (HasFlag(m_Desc.Usage, ResourceUsage::ShaderRead))
			bufferCI.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if (HasFlag(m_Desc.Usage, ResourceUsage::CopySrc))
			bufferCI.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		if (HasFlag(m_Desc.Usage, ResourceUsage::CopyDst))
			bufferCI.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		if (bufferCI.usage == 0)
			bufferCI.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		if (device->fnCreateBuffer(device->GetVkDevice(), &bufferCI, nullptr, &m_Buffer) != VK_SUCCESS)
		{
			CANDY_CORE_ERROR("VulkanBuffer: vkCreateBuffer failed for '{}'", m_Desc.DebugName);
			return;
		}

		VkMemoryRequirements memReqs;
		device->fnGetBufMemReqs(device->GetVkDevice(), m_Buffer, &memReqs);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize  = memReqs.size;
		allocInfo.memoryTypeIndex = FindMemoryType(device, memReqs.memoryTypeBits,
			m_Desc.CPUAccessible
				? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
				: VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (device->fnAllocateMemory(device->GetVkDevice(), &allocInfo, nullptr, &m_Memory) != VK_SUCCESS)
		{
			CANDY_CORE_ERROR("VulkanBuffer: vkAllocateMemory failed for '{}'", m_Desc.DebugName);
			return;
		}

		device->fnBindBufferMemory(device->GetVkDevice(), m_Buffer, m_Memory, 0);
	}

	uint32_t VulkanBuffer::FindMemoryType(VulkanDevice* device, uint32_t typeFilter, VkMemoryPropertyFlags props)
	{
		VkPhysicalDeviceMemoryProperties memProps;
		device->fnGetPhysicalDeviceMemoryProperties(device->GetVkPhysicalDevice(), &memProps);

		for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
		{
			if ((typeFilter & (1 << i)) &&
			    (memProps.memoryTypes[i].propertyFlags & props) == props)
				return i;
		}
		return 0;
	}

} // namespace Candy
