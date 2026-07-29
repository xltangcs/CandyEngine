#pragma once

#include "Runtime/Renderer/Texture.h"
#include "Runtime/RHI/RHIDevice.h"
#include <d3d12.h>
#include <memory>

namespace Candy {

	class DX12Device;
	class DX12Texture;

	// =========================================================================
	// DX12Texture2D — implements Texture2D backed by DX12Texture (RHI)
	//
	// Stores an SRV GPU descriptor handle for ImGui display via GetRendererID64.
	// Texture data is uploaded via DX12Texture::SetData.
	// =========================================================================
	class DX12Texture2D : public Texture2D
	{
	public:
		/// Create empty texture (width x height, RGBA8).
		DX12Texture2D(DX12Device* device, uint32_t width, uint32_t height);
		/// Load from file.
		DX12Texture2D(DX12Device* device, const std::string& path);
		virtual ~DX12Texture2D();

		uint32_t GetWidth()  const override { return m_Width; }
		uint32_t GetHeight() const override { return m_Height; }
		uint32_t GetRendererID() const override { return static_cast<uint32_t>(m_SRVGPUHandle.ptr & 0xFFFFFFFFull); }

		/// Returns the full 64-bit GPU descriptor handle for ImGui::Image.
		uint64_t GetRendererID64() const { return m_SRVGPUHandle.ptr; }

		void SetData(void* data, uint32_t size) override;

		void Bind(uint32_t slot = 0) const override;
		bool IsLoaded() const override { return m_IsLoaded; }

		bool operator==(const Texture& other) const override
		{
			return m_SRVGPUHandle.ptr == static_cast<const DX12Texture2D&>(other).m_SRVGPUHandle.ptr;
		}

		[[nodiscard]] DX12Texture*       GetRHI()       { return m_RHI.get(); }
		[[nodiscard]] const DX12Texture* GetRHI() const { return m_RHI.get(); }

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle() const { return m_SRVGPUHandle; }

	private:
		void AllocateSRV();

		DX12Device*                           m_Device = nullptr;
		std::unique_ptr<DX12Texture>          m_RHI;
		D3D12_GPU_DESCRIPTOR_HANDLE           m_SRVGPUHandle = {};
		std::string                           m_Path;
		bool                                  m_IsLoaded = false;
		uint32_t                              m_Width = 0, m_Height = 0;
		uint32_t                              m_SRVSlot = 0; // descriptor heap slot
	};

} // namespace Candy
