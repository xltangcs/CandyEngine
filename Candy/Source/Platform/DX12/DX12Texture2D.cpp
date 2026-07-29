#include "CandyPCH.h"
#include <Windows.h>
#include <d3d12.h>

#include "Platform/DX12/DX12Texture2D.h"
#include "Platform/DX12/DX12Texture.h"
#include "Platform/DX12/DX12Device.h"
#include "Runtime/Core/FileSystem.h"
#include "Runtime/Core/Log.h"

#include <stb_image.h>
#include <atomic>

namespace Candy {

	// =========================================================================
	// SRV slot allocator — simple atomic counter, descriptors 160–255
	// =========================================================================
	static std::atomic<uint32_t> s_NextSRVSlot{ 160 };

	static uint32_t AllocateSRVSlot()
	{
		uint32_t slot = s_NextSRVSlot.fetch_add(1);
		CANDY_CORE_ASSERT(slot < 256, "DX12Texture2D: SRV descriptor heap exhausted!");
		return slot;
	}

	// =========================================================================
	// Constructor: empty texture (e.g. white 1x1 pixel for Renderer2D)
	// =========================================================================

	DX12Texture2D::DX12Texture2D(DX12Device* device, uint32_t width, uint32_t height)
		: m_Device(device), m_Width(width), m_Height(height)
	{
		TextureDesc desc;
		desc.Width  = width;
		desc.Height = height;
		desc.Format = RHIFormat::R8G8B8A8Unorm;
		desc.Usage  = ResourceUsage::ShaderRead | ResourceUsage::CopyDst;
		desc.DebugName = "DX12Texture2D_" + std::to_string(width) + "x" + std::to_string(height);

		m_RHI = std::make_unique<DX12Texture>(device->GetNativeDevice(), desc);
		if (m_RHI->GetResource())
		{
			AllocateSRV();
			m_IsLoaded = true;
		}
	}

	// =========================================================================
	// Constructor: load from file
	// =========================================================================

	DX12Texture2D::DX12Texture2D(DX12Device* device, const std::string& path)
		: m_Device(device), m_Path(path)
	{
		// Read file (supports VFS)
		int width = 0, height = 0, channels = 0;
		stbi_uc* data = nullptr;

		// Try VFS first, then disk
		if (FileSystem::Get().Exists(path))
		{
			auto fileData = FileSystem::Get().Read(path);
			if (fileData)
			{
				data = stbi_load_from_memory(
					reinterpret_cast<const stbi_uc*>(fileData->data()),
					static_cast<int>(fileData->size()),
					&width, &height, &channels, 4); // force RGBA
			}
		}
		else
		{
			data = stbi_load(path.c_str(), &width, &height, &channels, 4);
		}

		if (!data)
		{
			CANDY_CORE_ERROR("DX12Texture2D: failed to load '{}'", path);
			return;
		}

		m_Width  = static_cast<uint32_t>(width);
		m_Height = static_cast<uint32_t>(height);

		TextureDesc desc;
		desc.Width     = m_Width;
		desc.Height    = m_Height;
		desc.Format    = RHIFormat::R8G8B8A8Unorm;
		desc.Usage     = ResourceUsage::ShaderRead | ResourceUsage::CopyDst;
		desc.MipLevels = 1;
		desc.DebugName = path;

		m_RHI = std::make_unique<DX12Texture>(device->GetNativeDevice(), desc);
		if (!m_RHI->GetResource())
		{
			stbi_image_free(data);
			CANDY_CORE_ERROR("DX12Texture2D: resource creation failed for '{}'", path);
			return;
		}

		// Upload pixel data
		uint32_t rowPitch = m_Width * 4; // RGBA
		m_RHI->SetData(data, rowPitch);
		stbi_image_free(data);

		AllocateSRV();
		m_IsLoaded = true;

		CANDY_CORE_INFO("DX12Texture2D: loaded '{}' ({}x{})", path, m_Width, m_Height);
	}

	DX12Texture2D::~DX12Texture2D()
	{
		m_RHI.reset();
	}

	// =========================================================================
	// SRV allocation — write SRV descriptor into device CBV_SRV_UAV heap
	// =========================================================================

	void DX12Texture2D::AllocateSRV()
	{
		if (!m_Device || !m_RHI)
			return;

		m_SRVSlot = AllocateSRVSlot();

		ID3D12DescriptorHeap* heap = m_Device->GetCBVSRVUAVHeap();
		if (!heap) return;

		uint32_t descSize = m_Device->GetCBVSRVDescriptorSize();
		m_RHI->CreateSRV(heap, m_SRVSlot, descSize);

		// Store GPU handle
		m_SRVGPUHandle = heap->GetGPUDescriptorHandleForHeapStart();
		m_SRVGPUHandle.ptr += static_cast<SIZE_T>(m_SRVSlot) * descSize;
	}

	// =========================================================================
	// SetData — update texture data
	// =========================================================================

	void DX12Texture2D::SetData(void* data, uint32_t size)
	{
		if (!m_RHI || !data)
			return;

		uint32_t expectedSize = m_Width * m_Height * 4; // RGBA
		uint32_t rowPitch = m_Width * 4;
		m_RHI->SetData(data, rowPitch);
	}

	// =========================================================================
	// Bind — set texture on a slot (for Renderer2D batch)
	// =========================================================================

	void DX12Texture2D::Bind(uint32_t slot) const
	{
		// In DX12, texture binding happens via descriptor tables set in
		// the command buffer (SetTexture).  This is a no-op at the engine
		// level — the actual binding is done in Renderer2D::Flush().
	}

} // namespace Candy
