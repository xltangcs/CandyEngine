#include "CandyPCH.h"
#include <Windows.h>
#include <d3d12.h>

#include "Platform/DX12/DX12Framebuffer.h"
#include "Platform/DX12/DX12Device.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	static const uint32_t s_MaxFramebufferSize = 8192;

	// =========================================================================
	// Helpers
	// =========================================================================

	static bool IsDepthFormat(FramebufferTextureFormat format)
	{
		switch (format)
		{
		case FramebufferTextureFormat::DEPTH24STENCIL8: return true;
		default: return false;
		}
	}

	// =========================================================================
	// Constructor / Destructor
	// =========================================================================

	DX12Framebuffer::DX12Framebuffer(const FramebufferSpecification& spec, DX12Device* device)
		: m_Specification(spec), m_Device(device)
	{
		for (auto& attachmentSpec : m_Specification.Attachments.Attachments)
		{
			if (!IsDepthFormat(attachmentSpec.TextureFormat))
				m_ColorAttachmentSpecs.push_back(attachmentSpec);
			else
				m_DepthAttachmentSpec = attachmentSpec;
		}
		Invalidate();
	}

	DX12Framebuffer::~DX12Framebuffer()
	{
		if (m_Specification.SwapChainTarget)
			return;

		m_ColorAttachments.clear();
		m_DepthAttachment.Reset();
		m_RTVHeap.Reset();
		m_DSVHeap.Reset();
		m_ColorSRVGPUHandles.clear();
		m_ReadbackBuffer.Reset();

		CANDY_CORE_INFO("DX12Framebuffer: destroyed");
	}

	// =========================================================================
	// Format mapping
	// =========================================================================

	DXGI_FORMAT DX12Framebuffer::MapFormat(FramebufferTextureFormat format) const
	{
		switch (format)
		{
		case FramebufferTextureFormat::RGBA8:           return DXGI_FORMAT_R8G8B8A8_UNORM;
		case FramebufferTextureFormat::RED_INTEGER:     return DXGI_FORMAT_R32_SINT;
		case FramebufferTextureFormat::DEPTH24STENCIL8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
		default:                                        return DXGI_FORMAT_UNKNOWN;
		}
	}

	// =========================================================================
	// Invalidate — (re)create all resources
	// =========================================================================

	void DX12Framebuffer::Invalidate()
	{
		if (m_Specification.SwapChainTarget)
		{
			m_ColorAttachments.clear();
			m_DepthAttachment.Reset();
			m_RTVHeap.Reset();
			m_DSVHeap.Reset();
			m_ColorSRVGPUHandles.clear();
			return;
		}

		ID3D12Device* nativeDevice = m_Device ? m_Device->GetNativeDevice() : nullptr;
		if (!nativeDevice)
		{
			CANDY_CORE_ERROR("DX12Framebuffer::Invalidate: null device");
			return;
		}

		// Release old resources
		m_ColorAttachments.clear();
		m_DepthAttachment.Reset();
		m_ColorSRVGPUHandles.clear();
		m_ReadbackBuffer.Reset();
		m_ReadbackBufferSize = 0;

		uint32_t width  = m_Specification.Width;
		uint32_t height = m_Specification.Height;
		uint32_t colorCount = static_cast<uint32_t>(m_ColorAttachmentSpecs.size());

		// ---- RTV descriptor heap -------------------------------------------
		{
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvHeapDesc.NumDescriptors = std::max(colorCount, 1u) + 1u; // +1 for depth readback copy target
			rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			rtvHeapDesc.NodeMask       = 0;

			HRESULT hr = nativeDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_RTVHeap));
			if (FAILED(hr))
			{
				CANDY_CORE_ERROR("DX12Framebuffer: RTV heap creation failed");
				return;
			}
			m_RTVDescriptorSize = nativeDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		// ---- DSV descriptor heap -------------------------------------------
		if (m_DepthAttachmentSpec.TextureFormat != FramebufferTextureFormat::None)
		{
			D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
			dsvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			dsvHeapDesc.NumDescriptors = 1;
			dsvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			dsvHeapDesc.NodeMask       = 0;

			HRESULT hr = nativeDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_DSVHeap));
			if (FAILED(hr))
			{
				CANDY_CORE_ERROR("DX12Framebuffer: DSV heap creation failed");
				return;
			}
			m_DSVDescriptorSize = nativeDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		}

		// ---- Create color textures + RTVs + SRVs ---------------------------
		m_ColorAttachments.resize(colorCount);
		m_ColorSRVGPUHandles.resize(colorCount);

		for (uint32_t i = 0; i < colorCount; ++i)
			CreateColorTexture(i, m_ColorAttachmentSpecs[i].TextureFormat);

		// ---- Create depth texture + DSV ------------------------------------
		if (m_DepthAttachmentSpec.TextureFormat != FramebufferTextureFormat::None)
			CreateDepthTexture();

		CANDY_CORE_INFO("DX12Framebuffer: created {}x{} with {} color + {} depth attachments",
		                width, height, colorCount,
		                (m_DepthAttachmentSpec.TextureFormat != FramebufferTextureFormat::None) ? 1 : 0);
	}

	void DX12Framebuffer::CreateColorTexture(uint32_t index, FramebufferTextureFormat format)
	{
		ID3D12Device* nativeDevice = m_Device->GetNativeDevice();
		uint32_t width  = m_Specification.Width;
		uint32_t height = m_Specification.Height;
		DXGI_FORMAT dxgiFormat = MapFormat(format);

		// --- Committed resource for the color attachment ---
		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type                 = D3D12_HEAP_TYPE_DEFAULT;
		heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resDesc.Width              = width;
		resDesc.Height             = height;
		resDesc.DepthOrArraySize   = 1;
		resDesc.MipLevels          = 1;
		resDesc.Format             = dxgiFormat;
		resDesc.SampleDesc.Count   = m_Specification.Samples;
		resDesc.SampleDesc.Quality = 0;
		resDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = dxgiFormat;
		// Default clear to black for color, entity ID clear to -1
		if (format == FramebufferTextureFormat::RED_INTEGER)
			clearValue.Color[0] = clearValue.Color[1] = clearValue.Color[2] = clearValue.Color[3] = -1.0f;

		HRESULT hr = nativeDevice->CreateCommittedResource(
			&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue,
			IID_PPV_ARGS(&m_ColorAttachments[index]));

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("DX12Framebuffer: color attachment {} creation failed", index);
			return;
		}

		// --- RTV ---
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RTVHeap->GetCPUDescriptorHandleForHeapStart();
		rtvHandle.ptr += static_cast<SIZE_T>(index) * m_RTVDescriptorSize;
		nativeDevice->CreateRenderTargetView(m_ColorAttachments[index].Get(), nullptr, rtvHandle);

		// --- SRV (in device's shared CBV_SRV_UAV heap, for ImGui display) ---
		ID3D12DescriptorHeap* srvHeap = m_Device->GetCBVSRVUAVHeap();
		if (srvHeap)
		{
			uint32_t srvDescriptorSize = m_Device->GetCBVSRVDescriptorSize();

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format                  = dxgiFormat;
			srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels     = 1;

			// Allocate from a fixed slot (simple approach: use index-based slots
			// starting at descriptor 128 to avoid colliding with ImGui's font SRV)
			D3D12_CPU_DESCRIPTOR_HANDLE cpuSrvHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
			cpuSrvHandle.ptr += static_cast<SIZE_T>(128 + index) * srvDescriptorSize;

			nativeDevice->CreateShaderResourceView(m_ColorAttachments[index].Get(), &srvDesc, cpuSrvHandle);

			// Store GPU handle for ImGui
			D3D12_GPU_DESCRIPTOR_HANDLE gpuSrvHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();
			gpuSrvHandle.ptr += static_cast<SIZE_T>(128 + index) * srvDescriptorSize;
			m_ColorSRVGPUHandles[index] = gpuSrvHandle;
		}
	}

	void DX12Framebuffer::CreateDepthTexture()
	{
		ID3D12Device* nativeDevice = m_Device->GetNativeDevice();
		uint32_t width  = m_Specification.Width;
		uint32_t height = m_Specification.Height;
		DXGI_FORMAT dxgiFormat = MapFormat(m_DepthAttachmentSpec.TextureFormat);

		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resDesc.Width            = width;
		resDesc.Height           = height;
		resDesc.DepthOrArraySize = 1;
		resDesc.MipLevels        = 1;
		resDesc.Format           = dxgiFormat;
		resDesc.SampleDesc.Count   = m_Specification.Samples;
		resDesc.SampleDesc.Quality = 0;
		resDesc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format               = dxgiFormat;
		clearValue.DepthStencil.Depth   = 1.0f;
		clearValue.DepthStencil.Stencil = 0;

		HRESULT hr = nativeDevice->CreateCommittedResource(
			&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue,
			IID_PPV_ARGS(&m_DepthAttachment));

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("DX12Framebuffer: depth attachment creation failed");
			return;
		}

		// DSV
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format        = dxgiFormat;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags         = D3D12_DSV_FLAG_NONE;

		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();
		nativeDevice->CreateDepthStencilView(m_DepthAttachment.Get(), &dsvDesc, dsvHandle);
	}

	// =========================================================================
	// Bind / Unbind — integrate with DX12 render pass
	// =========================================================================

	void DX12Framebuffer::Bind()
	{
		// In DX12, "bind" means setting this framebuffer as the active render
		// target for subsequent render pass recording.  The actual render target
		// binding happens in BeginRenderPass when this framebuffer is passed.
		// For now this is a no-op; the EditorLayer's rendering flow will be
		// updated to pass the framebuffer to the command buffer's BeginRenderPass.
	}

	void DX12Framebuffer::Unbind()
	{
		// No-op in DX12 — render pass ends via EndRenderPass.
	}

	// =========================================================================
	// Resize
	// =========================================================================

	void DX12Framebuffer::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || width > s_MaxFramebufferSize || height > s_MaxFramebufferSize)
		{
			CANDY_CORE_WARN("DX12Framebuffer: invalid resize dimensions {}x{}", width, height);
			return;
		}

		m_Specification.Width  = width;
		m_Specification.Height = height;

		if (m_Specification.SwapChainTarget)
			return;

		Invalidate();
		CANDY_CORE_INFO("DX12Framebuffer: resized to {}x{}", width, height);
	}

	// =========================================================================
	// ReadPixel — entity picking via readback
	// =========================================================================

	int DX12Framebuffer::ReadPixel(uint32_t attachmentIndex, int x, int y)
	{
		CANDY_CORE_ASSERT(!m_Specification.SwapChainTarget);
		CANDY_CORE_ASSERT(attachmentIndex < m_ColorAttachments.size());

		ID3D12Device* nativeDevice = m_Device->GetNativeDevice();
		if (!nativeDevice)
			return -1;

		auto* colorResource = m_ColorAttachments[attachmentIndex].Get();
		if (!colorResource)
			return -1;

		// Determine pixel size
		auto& spec = m_ColorAttachmentSpecs[attachmentIndex];
		uint32_t pixelSize = 4; // RGBA8 = 4 bytes, R32 = 4 bytes
		uint32_t rowPitch = m_Specification.Width * pixelSize;
		rowPitch = (rowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
		uint64_t bufferSize = static_cast<uint64_t>(rowPitch) * m_Specification.Height;

		// Create or resize readback buffer
		if (!m_ReadbackBuffer || m_ReadbackBufferSize < bufferSize)
		{
			m_ReadbackBuffer.Reset();

			D3D12_HEAP_PROPERTIES heapProps = {};
			heapProps.Type                 = D3D12_HEAP_TYPE_READBACK;
			heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			D3D12_RESOURCE_DESC bufDesc = {};
			bufDesc.Dimension    = D3D12_RESOURCE_DIMENSION_BUFFER;
			bufDesc.Width        = bufferSize;
			bufDesc.Height       = 1;
			bufDesc.DepthOrArraySize = 1;
			bufDesc.MipLevels    = 1;
			bufDesc.Format       = DXGI_FORMAT_UNKNOWN;
			bufDesc.Layout       = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			bufDesc.SampleDesc.Count = 1;

			HRESULT hr = nativeDevice->CreateCommittedResource(
				&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
				D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
				IID_PPV_ARGS(&m_ReadbackBuffer));

			if (FAILED(hr))
			{
				CANDY_CORE_ERROR("DX12Framebuffer::ReadPixel: readback buffer creation failed");
				return -1;
			}
			m_ReadbackBufferSize = bufferSize;
		}

		// --- Copy color resource → readback buffer ---
		ComPtr<ID3D12CommandAllocator> tempAllocator;
		HRESULT hr = nativeDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAllocator));
		if (FAILED(hr)) return -1;

		ComPtr<ID3D12GraphicsCommandList> tempCmdList;
		hr = nativeDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		                                     tempAllocator.Get(), nullptr,
		                                     IID_PPV_ARGS(&tempCmdList));
		if (FAILED(hr)) return -1;

		// Transition source to COPY_SOURCE
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource   = colorResource;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
		barrier.Transition.Subresource = 0;
		tempCmdList->ResourceBarrier(1, &barrier);

		// Copy texture → buffer (single pixel would be inefficient, copy whole texture)
		D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
		dstLoc.pResource       = m_ReadbackBuffer.Get();
		dstLoc.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dstLoc.PlacedFootprint.Offset    = 0;
		dstLoc.PlacedFootprint.Footprint.Format   = MapFormat(spec.TextureFormat);
		dstLoc.PlacedFootprint.Footprint.Width    = m_Specification.Width;
		dstLoc.PlacedFootprint.Footprint.Height   = m_Specification.Height;
		dstLoc.PlacedFootprint.Footprint.Depth    = 1;
		dstLoc.PlacedFootprint.Footprint.RowPitch = rowPitch;

		D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
		srcLoc.pResource        = colorResource;
		srcLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		srcLoc.SubresourceIndex = 0;

		tempCmdList->CopyTextureRegion(&dstLoc, x, y, 0, &srcLoc, nullptr);

		// Transition source back to RENDER_TARGET
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
		tempCmdList->ResourceBarrier(1, &barrier);

		tempCmdList->Close();

		// Execute and wait
		ID3D12CommandList* lists[] = { tempCmdList.Get() };
		m_Device->GetNativeQueue()->ExecuteCommandLists(1, lists);
		m_Device->WaitIdle();

		// --- Read the pixel ---
		void* mapped = nullptr;
		D3D12_RANGE readRange = { 0, bufferSize };
		hr = m_ReadbackBuffer->Map(0, &readRange, &mapped);
		if (FAILED(hr) || !mapped)
			return -1;

		// Read the pixel at the beginning of the 1x1 copy region (x, y)
		int pixelValue = static_cast<int*>(mapped)[0];

		D3D12_RANGE writeRange = { 0, 0 };
		m_ReadbackBuffer->Unmap(0, &writeRange);

		return pixelValue;
	}

	// =========================================================================
	// ClearAttachment — clear a single color attachment
	// =========================================================================

	void DX12Framebuffer::ClearAttachment(uint32_t attachmentIndex, int value)
	{
		CANDY_CORE_ASSERT(!m_Specification.SwapChainTarget);
		CANDY_CORE_ASSERT(attachmentIndex < m_ColorAttachments.size());

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetRTVHandle(attachmentIndex);
		FLOAT clearColor[4] = {
			static_cast<FLOAT>(value),
			0.0f, 0.0f, 0.0f
		};

		// We need a command list to clear — create a temp one
		ID3D12Device* nativeDevice = m_Device->GetNativeDevice();
		if (!nativeDevice) return;

		ComPtr<ID3D12CommandAllocator> tempAllocator;
		HRESULT hr = nativeDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAllocator));
		if (FAILED(hr)) return;

		ComPtr<ID3D12GraphicsCommandList> tempCmdList;
		hr = nativeDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		                                     tempAllocator.Get(), nullptr,
		                                     IID_PPV_ARGS(&tempCmdList));
		if (FAILED(hr)) return;

		tempCmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		tempCmdList->Close();

		ID3D12CommandList* lists[] = { tempCmdList.Get() };
		m_Device->GetNativeQueue()->ExecuteCommandLists(1, lists);
		m_Device->WaitIdle();
	}

	// =========================================================================
	// Accessors
	// =========================================================================

	uint32_t DX12Framebuffer::GetColorAttachmentRendererID(uint32_t index) const
	{
		return static_cast<uint32_t>(GetColorAttachmentGPUHandle(index) & 0xFFFFFFFFull);
	}

	uint64_t DX12Framebuffer::GetColorAttachmentGPUHandle(uint32_t index) const
	{
		if (m_Specification.SwapChainTarget)
			return 0;

		if (index >= m_ColorSRVGPUHandles.size())
			return 0;

		// Return full 64-bit GPU descriptor handle .ptr
		// In DX12 ImGui::Image path, this is cast to (ImTextureID)(uint64_t)ptr
		return m_ColorSRVGPUHandles[index].ptr;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DX12Framebuffer::GetRTVHandle(uint32_t index) const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle = m_RTVHeap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<SIZE_T>(index) * m_RTVDescriptorSize;
		return handle;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DX12Framebuffer::GetDSVHandle() const
	{
		if (!m_DSVHeap)
			return {};
		return m_DSVHeap->GetCPUDescriptorHandleForHeapStart();
	}

	bool DX12Framebuffer::HasDepthAttachment() const
	{
		return m_DepthAttachmentSpec.TextureFormat != FramebufferTextureFormat::None;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE DX12Framebuffer::GetColorSRVGPUHandle(uint32_t index) const
	{
		if (index < m_ColorSRVGPUHandles.size())
			return m_ColorSRVGPUHandles[index];
		return {};
	}

} // namespace Candy
