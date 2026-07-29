#include "CandyPCH.h"
#include <Windows.h>
#include <vulkan/vulkan.h>

#include "Platform/Vulkan/VulkanTexture2D.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Runtime/Core/FileSystem.h"
#include "Runtime/Core/Log.h"

#include <stb_image.h>

namespace Candy {

	// =========================================================================
	// Helper: find a memory type index
	// =========================================================================
	static uint32_t FindMemoryType(VulkanDevice* dev, uint32_t typeFilter, VkMemoryPropertyFlags props)
	{
		VkPhysicalDeviceMemoryProperties memProps;
		vkGetPhysicalDeviceMemoryProperties(dev->GetVkPhysicalDevice(), &memProps);
		for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
			if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
				return i;
		return UINT32_MAX;
	}

	// =========================================================================
	// Constructor: empty texture
	// =========================================================================
	VulkanTexture2D::VulkanTexture2D(VulkanDevice* device, uint32_t width, uint32_t height)
		: m_Device(device), m_Width(width), m_Height(height)
	{
		CreateImage(width, height);
		CreateImageView();
		CreateSampler();
		m_IsLoaded = (m_Image != VK_NULL_HANDLE);
	}

	// =========================================================================
	// Constructor: load from file
	// =========================================================================
	VulkanTexture2D::VulkanTexture2D(VulkanDevice* device, const std::string& path)
		: m_Device(device), m_Path(path)
	{
		int width = 0, height = 0, channels = 0;
		stbi_uc* data = nullptr;

		if (FileSystem::Get().Exists(path))
		{
			auto fileData = FileSystem::Get().Read(path);
			if (fileData)
				data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(fileData->data()),
				                             static_cast<int>(fileData->size()), &width, &height, &channels, 4);
		}
		else
		{
			data = stbi_load(path.c_str(), &width, &height, &channels, 4);
		}

		if (!data)
		{
			CANDY_CORE_ERROR("VulkanTexture2D: failed to load '{}'", path);
			return;
		}

		m_Width  = static_cast<uint32_t>(width);
		m_Height = static_cast<uint32_t>(height);

		CreateImage(m_Width, m_Height);
		CreateImageView();

		// Upload pixel data
		uint32_t dataSize = m_Width * m_Height * 4;
		UploadData(data, dataSize);
		stbi_image_free(data);

		CreateSampler();
		m_IsLoaded = true;
		CANDY_CORE_INFO("VulkanTexture2D: loaded '{}' ({}x{})", path, m_Width, m_Height);
	}

	VulkanTexture2D::~VulkanTexture2D()
	{
		VkDevice dev = m_Device ? m_Device->GetVkDevice() : VK_NULL_HANDLE;
		if (m_Sampler   && m_Device->fnDestroySampler)    m_Device->fnDestroySampler(dev, m_Sampler, nullptr);
		if (m_ImageView && m_Device->fnDestroyImageView)  m_Device->fnDestroyImageView(dev, m_ImageView, nullptr);
		if (m_Image     && m_Device->fnDestroyImage)      m_Device->fnDestroyImage(dev, m_Image, nullptr);
		if (m_Memory    && m_Device->fnFreeMemory)         m_Device->fnFreeMemory(dev, m_Memory, nullptr);
	}

	// =========================================================================
	// CreateImage — VkImage + VkDeviceMemory
	// =========================================================================
	void VulkanTexture2D::CreateImage(uint32_t w, uint32_t h)
	{
		VkDevice dev = m_Device->GetVkDevice();

		VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		ici.imageType     = VK_IMAGE_TYPE_2D;
		ici.format        = VK_FORMAT_R8G8B8A8_UNORM;
		ici.extent        = { w, h, 1 };
		ici.mipLevels     = 1;
		ici.arrayLayers   = 1;
		ici.samples       = VK_SAMPLE_COUNT_1_BIT;
		ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
		ici.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
		ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if (m_Device->fnCreateImage(dev, &ici, nullptr, &m_Image) != VK_SUCCESS)
		{
			CANDY_CORE_ERROR("VulkanTexture2D::CreateImage failed");
			return;
		}

		// Allocate memory
		VkMemoryRequirements memReqs;
		m_Device->fnGetImageMemoryRequirements(dev, m_Image, &memReqs);
		uint32_t memType = FindMemoryType(m_Device, memReqs.memoryTypeBits,
		                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		ai.allocationSize  = memReqs.size;
		ai.memoryTypeIndex = memType;

		if (m_Device->fnAllocateMemory(dev, &ai, nullptr, &m_Memory) != VK_SUCCESS)
		{
			CANDY_CORE_ERROR("VulkanTexture2D: memory allocation failed");
			return;
		}

		m_Device->fnBindImageMemory(dev, m_Image, m_Memory, 0);
	}

	// =========================================================================
	// CreateImageView
	// =========================================================================
	void VulkanTexture2D::CreateImageView()
	{
		VkImageViewCreateInfo ivci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		ivci.image                           = m_Image;
		ivci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
		ivci.format                          = VK_FORMAT_R8G8B8A8_UNORM;
		ivci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		ivci.subresourceRange.baseMipLevel   = 0;
		ivci.subresourceRange.levelCount     = 1;
		ivci.subresourceRange.baseArrayLayer = 0;
		ivci.subresourceRange.layerCount     = 1;

		if (m_Device->fnCreateImageView(m_Device->GetVkDevice(), &ivci, nullptr, &m_ImageView) != VK_SUCCESS)
			CANDY_CORE_ERROR("VulkanTexture2D: image view creation failed");
	}

	// =========================================================================
	// CreateSampler
	// =========================================================================
	void VulkanTexture2D::CreateSampler()
	{
		VkSamplerCreateInfo sci = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		sci.magFilter    = VK_FILTER_LINEAR;
		sci.minFilter    = VK_FILTER_LINEAR;
		sci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.maxLod       = 1.0f;

		if (m_Device->fnCreateSampler(m_Device->GetVkDevice(), &sci, nullptr, &m_Sampler) != VK_SUCCESS)
			CANDY_CORE_ERROR("VulkanTexture2D: sampler creation failed");
	}

	// =========================================================================
	// UploadData — staging buffer → image copy
	// =========================================================================
	void VulkanTexture2D::UploadData(const void* data, uint32_t size)
	{
		VkDevice dev = m_Device->GetVkDevice();
		if (!data || size == 0) return;

		// Create staging buffer
		VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bci.size  = size;
		bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VkBuffer stagingBuf;
		if (m_Device->fnCreateBuffer(dev, &bci, nullptr, &stagingBuf) != VK_SUCCESS) return;

		VkMemoryRequirements memReqs;
		m_Device->fnGetBufMemReqs(dev, stagingBuf, &memReqs);
		uint32_t memType = FindMemoryType(m_Device, memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		ai.allocationSize = memReqs.size;
		ai.memoryTypeIndex = memType;

		VkDeviceMemory stagingMem;
		if (m_Device->fnAllocateMemory(dev, &ai, nullptr, &stagingMem) != VK_SUCCESS)
		{
			m_Device->fnDestroyBuffer(dev, stagingBuf, nullptr);
			return;
		}
		m_Device->fnBindBufferMemory(dev, stagingBuf, stagingMem, 0);

		// Map and copy
		void* mapped = nullptr;
		m_Device->fnMapMemory(dev, stagingMem, 0, size, 0, &mapped);
		memcpy(mapped, data, size);
		m_Device->fnUnmapMemory(dev, stagingMem);

		// --- One-shot command buffer for copy ---
		VkCommandBufferAllocateInfo cbai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		cbai.commandPool        = m_Device->GetVkCommandPool();
		cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cbai.commandBufferCount = 1;

		VkCommandBuffer cb;
		m_Device->fnAllocateCommandBuffers(dev, &cbai, &cb);

		VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		m_Device->fnBeginCommandBuffer(cb, &beginInfo);

		// Transition image to TRANSFER_DST_OPTIMAL
		{
			VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image               = m_Image;
			barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			barrier.srcAccessMask       = 0;
			barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;

			m_Device->fnCmdPipelineBarrier(cb,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &barrier);
		}

		// Copy buffer to image
		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset      = 0;
		copyRegion.bufferRowLength   = m_Width;
		copyRegion.bufferImageHeight = m_Height;
		copyRegion.imageSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copyRegion.imageExtent       = { m_Width, m_Height, 1 };

		m_Device->fnCmdCopyBufferToImage(cb, stagingBuf, m_Image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		// Transition to SHADER_READ_ONLY_OPTIMAL
		{
			VkImageMemoryBarrier barrier2 = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			barrier2.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier2.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier2.image               = m_Image;
			barrier2.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			barrier2.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier2.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;

			m_Device->fnCmdPipelineBarrier(cb,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &barrier2);
		}

		m_Device->fnEndCommandBuffer(cb);

		// Submit and wait
		VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		si.commandBufferCount = 1;
		si.pCommandBuffers    = &cb;

		m_Device->fnQueueSubmit(m_Device->GetVkQueue(), 1, &si, VK_NULL_HANDLE);
		m_Device->WaitIdle();

		// Cleanup staging
		m_Device->fnFreeCommandBuffers(dev, cbai.commandPool, 1, &cb);
		m_Device->fnDestroyBuffer(dev, stagingBuf, nullptr);
		m_Device->fnFreeMemory(dev, stagingMem, nullptr);

		CANDY_CORE_INFO("VulkanTexture2D: uploaded {}x{} ({} bytes)", m_Width, m_Height, size);
	}

	// =========================================================================
	// SetData / Bind
	// =========================================================================
	void VulkanTexture2D::SetData(void* data, uint32_t size)
	{
		UploadData(data, size);
	}

	void VulkanTexture2D::Bind(uint32_t slot) const
	{
		// In Vulkan, binding happens via descriptor sets in the command buffer
	}

} // namespace Candy
