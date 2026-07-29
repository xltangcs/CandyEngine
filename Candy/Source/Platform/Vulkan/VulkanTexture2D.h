#pragma once

#include "Runtime/Renderer/Texture.h"
#include <vulkan/vulkan.h>
#include <memory>

namespace Candy {

	class VulkanDevice;

	// =========================================================================
	// VulkanTexture2D — VkImage + VkImageView + VkSampler backed Texture2D
	// =========================================================================
	class VulkanTexture2D : public Texture2D
	{
	public:
		VulkanTexture2D(VulkanDevice* device, uint32_t width, uint32_t height);
		VulkanTexture2D(VulkanDevice* device, const std::string& path);
		virtual ~VulkanTexture2D();

		uint32_t GetWidth()  const override { return m_Width; }
		uint32_t GetHeight() const override { return m_Height; }
		uint32_t GetRendererID() const override { return 0; }
		uint64_t GetRendererID64() const override { return reinterpret_cast<uint64_t>(m_ImageView); }

		void SetData(void* data, uint32_t size) override;
		void Bind(uint32_t slot = 0) const override;
		bool IsLoaded() const override { return m_IsLoaded; }

		bool operator==(const Texture& other) const override
		{
			return m_ImageView == static_cast<const VulkanTexture2D&>(other).m_ImageView;
		}

		[[nodiscard]] VkImageView GetImageView() const { return m_ImageView; }
		[[nodiscard]] VkSampler   GetSampler()   const { return m_Sampler; }

	private:
		void CreateImage(uint32_t w, uint32_t h);
		void CreateImageView();
		void CreateSampler();
		void TransitionLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
		void UploadData(const void* data, uint32_t size);

		VulkanDevice* m_Device     = nullptr;
		VkImage       m_Image      = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory    = VK_NULL_HANDLE;
		VkImageView   m_ImageView  = VK_NULL_HANDLE;
		VkSampler     m_Sampler    = VK_NULL_HANDLE;

		std::string m_Path;
		bool        m_IsLoaded   = false;
		uint32_t    m_Width      = 0, m_Height = 0;
	};

} // namespace Candy
