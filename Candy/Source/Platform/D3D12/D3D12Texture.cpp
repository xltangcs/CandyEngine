#include "CandyPCH.h"
#include <Windows.h>
#include <d3d12.h>

#include "Platform/D3D12/D3D12Texture.h"
#include "Platform/D3D12/D3D12Device.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	// =========================================================================
	// Helper: Map RHIFormat → DXGI_FORMAT
	// =========================================================================
	static DXGI_FORMAT MapRHIFormatToDXGI(RHIFormat format)
	{
		switch (format)
		{
		case RHIFormat::R8Unorm:            return DXGI_FORMAT_R8_UNORM;
		case RHIFormat::R8G8Unorm:          return DXGI_FORMAT_R8G8_UNORM;
		case RHIFormat::R8G8B8A8Unorm:      return DXGI_FORMAT_R8G8B8A8_UNORM;
		case RHIFormat::R8G8B8A8Srgb:       return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		case RHIFormat::B8G8R8A8Unorm:      return DXGI_FORMAT_B8G8R8A8_UNORM;
		case RHIFormat::B8G8R8A8Srgb:       return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		case RHIFormat::R32Float:           return DXGI_FORMAT_R32_FLOAT;
		case RHIFormat::R32G32Float:        return DXGI_FORMAT_R32G32_FLOAT;
		case RHIFormat::R16G16B16A16Float:  return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case RHIFormat::R16G16Float:        return DXGI_FORMAT_R16G16_FLOAT;
		case RHIFormat::R32G32B32A32Float:  return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case RHIFormat::D24UnormS8Uint:     return DXGI_FORMAT_D24_UNORM_S8_UINT;
		case RHIFormat::D32Float:           return DXGI_FORMAT_D32_FLOAT;
		default:                            return DXGI_FORMAT_UNKNOWN;
		}
	}

	static D3D12_FILTER MapSamplerFilter(SamplerFilter min, SamplerFilter mag, SamplerFilter mip,
	                                     uint32_t maxAnisotropy)
	{
		if (maxAnisotropy > 1)
			return D3D12_FILTER_ANISOTROPIC;
		if (min == SamplerFilter::Nearest && mag == SamplerFilter::Nearest && mip == SamplerFilter::Nearest)
			return D3D12_FILTER_MIN_MAG_MIP_POINT;
		if (min == SamplerFilter::Linear && mag == SamplerFilter::Linear && mip == SamplerFilter::Nearest)
			return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		if (min == SamplerFilter::Nearest && mag == SamplerFilter::Nearest && mip == SamplerFilter::Linear)
			return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
		return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	}

	static D3D12_TEXTURE_ADDRESS_MODE MapAddressMode(SamplerAddressMode mode)
	{
		switch (mode)
		{
		case SamplerAddressMode::Repeat:        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		case SamplerAddressMode::ClampToEdge:   return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		case SamplerAddressMode::ClampToBorder: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		case SamplerAddressMode::MirroredRepeat:  return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		default:                                return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		}
	}

	// =========================================================================
	// D3D12Texture
	// =========================================================================

	D3D12Texture::D3D12Texture(D3D12Device* device, const TextureDesc& desc)
		: m_Desc(desc), m_Device(device)
	{
		ID3D12Device* nativeDevice = device ? device->GetNativeDevice() : nullptr;
		if (!nativeDevice)
			return;

		DXGI_FORMAT dxgiFormat = MapRHIFormatToDXGI(desc.Format);
		if (dxgiFormat == DXGI_FORMAT_UNKNOWN)
		{
			CANDY_CORE_ERROR("D3D12Texture: unsupported format");
			return;
		}

		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resDesc.Width            = desc.Width;
		resDesc.Height           = desc.Height;
		resDesc.DepthOrArraySize = (desc.Type == TextureType::Cubemap)
			? static_cast<UINT16>(6) : static_cast<UINT16>(desc.Depth);
		resDesc.MipLevels        = static_cast<UINT16>(desc.MipLevels);
		resDesc.Format           = dxgiFormat;
		resDesc.SampleDesc.Count   = desc.SampleCount;
		resDesc.SampleDesc.Quality = 0;
		resDesc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resDesc.Flags            = D3D12_RESOURCE_FLAG_NONE;

		if (HasFlag(desc.Usage, ResourceUsage::RenderTarget))
			resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		if (HasFlag(desc.Usage, ResourceUsage::DepthStencil))
			resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		if (HasFlag(desc.Usage, ResourceUsage::ShaderWrite))
			resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		if (HasFlag(desc.Usage, ResourceUsage::ShaderWrite))
			resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_CLEAR_VALUE* pClearValue = nullptr;
		D3D12_CLEAR_VALUE  clearValue  = {};
		if (resDesc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
		{
			clearValue.Format = dxgiFormat;
			if (resDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
			{
				clearValue.DepthStencil.Depth   = 1.0f;
				clearValue.DepthStencil.Stencil = 0;
			}
			pClearValue = &clearValue;
		}

		HRESULT hr = nativeDevice->CreateCommittedResource(
			&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
			D3D12_RESOURCE_STATE_COMMON, pClearValue,
			IID_PPV_ARGS(&m_Resource));

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12Texture: CreateCommittedResource failed for {}x{} texture (hr=0x{:08X})",
			                 desc.Width, desc.Height, static_cast<uint32_t>(hr));
			if (nativeDevice)
			{
				HRESULT removed = nativeDevice->GetDeviceRemovedReason();
				if (removed != S_OK)
					CANDY_CORE_ERROR("  -> GetDeviceRemovedReason = 0x{:08X}", static_cast<uint32_t>(removed));
			}
			return;
		}

		m_State = D3D12_RESOURCE_STATE_COMMON;

		CANDY_CORE_INFO("D3D12Texture: created {}x{} (format: {}, mips: {})",
		                desc.Width, desc.Height, static_cast<int>(desc.Format), desc.MipLevels);
	}

	D3D12Texture::D3D12Texture(D3D12Device* device, const TextureDesc& desc,
	                           Microsoft::WRL::ComPtr<ID3D12Resource> resource,
	                           D3D12_RESOURCE_STATES state)
		: m_Desc(desc), m_Device(device), m_Resource(std::move(resource)), m_State(state)
	{
	}

	Ref<D3D12Texture> D3D12Texture::Adopt(D3D12Device* device,
	                                      Microsoft::WRL::ComPtr<ID3D12Resource> resource,
	                                      const TextureDesc& desc,
	                                      D3D12_RESOURCE_STATES state)
	{
		if (!device || !resource)
			return nullptr;
		// `new` here runs inside the class scope so the private adopting
		// constructor is accessible (std::make_shared would not be).
		return Ref<D3D12Texture>(new D3D12Texture(device, desc, std::move(resource), state));
	}

	D3D12Texture::~D3D12Texture()
	{
		m_Resource.Reset();
	}

	void D3D12Texture::SetData(const void* data, uint32_t rowPitch, uint32_t slicePitch)
	{
		if (!data || !m_Resource || !m_Device)
			return;

		ID3D12Device* nativeDevice = m_Device->GetNativeDevice();
		ID3D12CommandQueue* queue = m_Device->GetNativeQueue();
		if (!nativeDevice || !queue)
			return;

		// Compute the D3D12 footprint for the texture. The upload buffer must be
		// sized for the footprint's ALIGNED row pitch (D3D12_TEXTURE_DATA_PITCH_
		// ALIGNMENT), not the raw row size — otherwise CopyTextureRegion reads past
		// the end of the buffer and the GPU hangs (device removed / DEVICE_HUNG).
		auto d3d12ResDesc = m_Resource->GetDesc();

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
		UINT64 requiredSize = 0;
		nativeDevice->GetCopyableFootprints(
			&d3d12ResDesc, 0, 1, 0,
			&footprint, nullptr, nullptr, &requiredSize);

		// Create upload buffer (sized for the footprint's aligned layout)
		D3D12_HEAP_PROPERTIES uploadHeapProps = {};
		uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC uploadDesc = {};
		uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		uploadDesc.Width     = requiredSize;
		uploadDesc.Height    = 1;
		uploadDesc.DepthOrArraySize = 1;
		uploadDesc.MipLevels = 1;
		uploadDesc.Format    = DXGI_FORMAT_UNKNOWN;
		uploadDesc.Layout    = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		uploadDesc.SampleDesc.Count = 1;

		Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
		HRESULT hr = nativeDevice->CreateCommittedResource(
			&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&uploadBuffer));

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12Texture::SetData: upload buffer creation failed");
			return;
		}

		// Copy data row-by-row at the footprint's aligned row pitch.
		uint32_t srcRowPitch = rowPitch > 0 ? rowPitch : m_Desc.Width * 4;
		uint32_t dstRowPitch = static_cast<uint32_t>(footprint.Footprint.RowPitch);
		void* mapped = nullptr;
		D3D12_RANGE readRange = { 0, 0 };
		uploadBuffer->Map(0, &readRange, &mapped);
		if (mapped)
		{
			for (uint32_t row = 0; row < m_Desc.Height; ++row)
			{
				memcpy(static_cast<uint8_t*>(mapped) + row * dstRowPitch,
				       static_cast<const uint8_t*>(data) + row * srcRowPitch,
				       srcRowPitch);
			}
			uploadBuffer->Unmap(0, nullptr);
		}

		// Copy upload → texture using temp command list
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> tempAllocator;
		hr = nativeDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAllocator));
		if (FAILED(hr)) return;

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> tempCmdList;
		hr = nativeDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		                                     tempAllocator.Get(), nullptr,
		                                     IID_PPV_ARGS(&tempCmdList));
		if (FAILED(hr)) return;

		// Transition texture to COPY_DEST
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource   = m_Resource.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
		barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		tempCmdList->ResourceBarrier(1, &barrier);

		// Copy
		D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
		dstLoc.pResource        = m_Resource.Get();
		dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstLoc.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
		srcLoc.pResource       = uploadBuffer.Get();
		srcLoc.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		srcLoc.PlacedFootprint = footprint;

		tempCmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

		// Transition texture to PIXEL_SHADER_RESOURCE
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		tempCmdList->ResourceBarrier(1, &barrier);

		tempCmdList->Close();

		// Execute the copy on the device queue and wait for it to finish. Without
		// this the pixel data never reaches the GPU — the texture stays empty and
		// every texture (icons, sprites, checkerboard) renders blank.
		ID3D12CommandList* lists[] = { tempCmdList.Get() };
		queue->ExecuteCommandLists(1, lists);
		uint64_t fenceValue = m_Device->SignalFence();
		m_Device->WaitForFenceValue(fenceValue);

		m_State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}

	void D3D12Texture::CreateSRV(ID3D12DescriptorHeap* heap, uint32_t slotIndex, uint32_t descriptorSize) const
	{
		if (!m_Resource || !m_Device || !heap)
			return;
		ID3D12Device* nativeDevice = m_Device->GetNativeDevice();
		if (!nativeDevice)
			return;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format                  = MapRHIFormatToDXGI(m_Desc.Format);
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		if (m_Desc.Type == TextureType::Cubemap)
		{
			// One descriptor covers all 6 faces (mip chain included).
			srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MipLevels   = m_Desc.MipLevels;
			srvDesc.TextureCube.MostDetailedMip = 0;
		}
		else
		{
			srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels     = m_Desc.MipLevels;
			srvDesc.Texture2D.MostDetailedMip = 0;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = heap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += static_cast<SIZE_T>(slotIndex) * descriptorSize;

		nativeDevice->CreateShaderResourceView(m_Resource.Get(), &srvDesc, cpuHandle);
	}

	bool D3D12Texture::WriteSubresources(const TextureSubresourceData* data, uint32_t count)
	{
		if (!data || !m_Resource || !m_Device)
			return false;

		ID3D12Device* nativeDevice = m_Device->GetNativeDevice();
		ID3D12CommandQueue* queue = m_Device->GetNativeQueue();
		if (!nativeDevice || !queue)
			return false;

		auto d3d12ResDesc = m_Resource->GetDesc();
		const uint32_t totalSubresources = d3d12ResDesc.MipLevels * d3d12ResDesc.DepthOrArraySize;
		if (count > totalSubresources)
			count = totalSubresources;
		if (count == 0)
			return false;

		// Footprints for every subresource; upload buffer holds them all.
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(count);
		std::vector<UINT> rowCounts(count);
		std::vector<UINT64> rowSizes(count);
		UINT64 requiredSize = 0;
		nativeDevice->GetCopyableFootprints(&d3d12ResDesc, 0, count, 0,
			footprints.data(), rowCounts.data(), rowSizes.data(), &requiredSize);

		D3D12_HEAP_PROPERTIES uploadHeapProps = {};
		uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC uploadDesc = {};
		uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		uploadDesc.Width     = requiredSize;
		uploadDesc.Height    = 1;
		uploadDesc.DepthOrArraySize = 1;
		uploadDesc.MipLevels = 1;
		uploadDesc.Format    = DXGI_FORMAT_UNKNOWN;
		uploadDesc.Layout    = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		uploadDesc.SampleDesc.Count = 1;

		Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
		HRESULT hr = nativeDevice->CreateCommittedResource(
			&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&uploadBuffer));
		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("D3D12Texture::WriteSubresources: upload buffer creation failed");
			return false;
		}

		void* mapped = nullptr;
		D3D12_RANGE readRange = { 0, 0 };
		uploadBuffer->Map(0, &readRange, &mapped);
		if (!mapped)
			return false;

		for (uint32_t i = 0; i < count; ++i)
		{
			const uint32_t dstRowPitch = static_cast<uint32_t>(footprints[i].Footprint.RowPitch);
			const uint32_t srcRowPitch = data[i].RowPitch > 0 ? data[i].RowPitch : static_cast<uint32_t>(rowSizes[i]);
			const uint32_t rowCount   = rowCounts[i];
			const uint8_t* srcSlice = static_cast<const uint8_t*>(data[i].Data);
			const uint32_t srcSlicePitch = (data[i].SlicePitch > 0) ? data[i].SlicePitch : srcRowPitch * rowCount;

			uint8_t* dstSlice = static_cast<uint8_t*>(mapped) + footprints[i].Offset;
			for (uint32_t row = 0; row < rowCount; ++row)
			{
				memcpy(dstSlice + row * dstRowPitch,
				       srcSlice + row * srcRowPitch,
				       srcRowPitch);
			}
			(void)srcSlicePitch;
		}
		uploadBuffer->Unmap(0, nullptr);

		// Execute the copy on a temp command list and wait (same pattern as SetData).
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> tempAllocator;
		hr = nativeDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAllocator));
		if (FAILED(hr)) return false;

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> tempCmdList;
		hr = nativeDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		                                     tempAllocator.Get(), nullptr,
		                                     IID_PPV_ARGS(&tempCmdList));
		if (FAILED(hr)) return false;

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource   = m_Resource.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
		barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		tempCmdList->ResourceBarrier(1, &barrier);

		for (uint32_t i = 0; i < count; ++i)
		{
			D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
			dstLoc.pResource        = m_Resource.Get();
			dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dstLoc.SubresourceIndex = i;

			D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
			srcLoc.pResource       = uploadBuffer.Get();
			srcLoc.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			srcLoc.PlacedFootprint = footprints[i];

			tempCmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
		}

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		tempCmdList->ResourceBarrier(1, &barrier);

		tempCmdList->Close();

		ID3D12CommandList* lists[] = { tempCmdList.Get() };
		queue->ExecuteCommandLists(1, lists);
		uint64_t fenceValue = m_Device->SignalFence();
		m_Device->WaitForFenceValue(fenceValue);

		m_State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		return true;
	}

	void D3D12Texture::CreateUAV(ID3D12DescriptorHeap* heap, uint32_t slotIndex, uint32_t descriptorSize,
	                             uint32_t mipSlice) const
	{
		if (!m_Resource || !m_Device || !heap)
			return;
		ID3D12Device* nativeDevice = m_Device->GetNativeDevice();
		if (!nativeDevice)
			return;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = MapRHIFormatToDXGI(m_Desc.Format);

		if (m_Desc.Type == TextureType::Cubemap)
		{
			// Cubemaps are UAV-accessed as Texture2DArray over the 6 faces
			// (D3D12 has no TextureCube UAV dimension).
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			uavDesc.Texture2DArray.ArraySize       = 6;
			uavDesc.Texture2DArray.FirstArraySlice = 0;
			uavDesc.Texture2DArray.MipSlice        = mipSlice;
		}
		else
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = mipSlice;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = heap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += static_cast<SIZE_T>(slotIndex) * descriptorSize;

		nativeDevice->CreateUnorderedAccessView(m_Resource.Get(), nullptr, &uavDesc, cpuHandle);
	}

	void D3D12Texture::Transition(ID3D12GraphicsCommandList* list, D3D12_RESOURCE_STATES to)
	{
		if (!m_Resource || !list || m_State == to)
			return;

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource   = m_Resource.Get();
		barrier.Transition.StateBefore = m_State;
		barrier.Transition.StateAfter  = to;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		list->ResourceBarrier(1, &barrier);
		m_State = to;
	}

	void D3D12Texture::TransitionSubresource(ID3D12GraphicsCommandList* list, uint32_t subresource,
	                                         D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
	{
		if (!m_Resource || !list)
			return;

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource   = m_Resource.Get();
		barrier.Transition.StateBefore = from;
		barrier.Transition.StateAfter  = to;
		barrier.Transition.Subresource = subresource;
		list->ResourceBarrier(1, &barrier);
	}

	void D3D12Texture::UAVBarrier(ID3D12GraphicsCommandList* list) const
	{
		if (!m_Resource || !list)
			return;

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type  = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.UAV.pResource = m_Resource.Get();
		list->ResourceBarrier(1, &barrier);
	}

	// =========================================================================
	// D3D12Sampler
	// =========================================================================

	D3D12Sampler::D3D12Sampler(ID3D12Device* device, ID3D12DescriptorHeap* samplerHeap,
	                         uint32_t descriptorSize, const SamplerDesc& desc)
		: m_Desc(desc), m_Device(device), m_SamplerHeap(samplerHeap)
	{
		if (!device || !samplerHeap)
			return;

		D3D12_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter         = MapSamplerFilter(desc.MinFilter, desc.MagFilter, desc.MipFilter, desc.MaxAnisotropy);
		samplerDesc.AddressU       = MapAddressMode(desc.AddressU);
		samplerDesc.AddressV       = MapAddressMode(desc.AddressV);
		samplerDesc.AddressW       = MapAddressMode(desc.AddressW);
		samplerDesc.MipLODBias     = desc.MipLodBias;
		samplerDesc.MaxAnisotropy  = desc.MaxAnisotropy;
		samplerDesc.ComparisonFunc = (desc.CompareOp != CompareOp::Never)
			? D3D12_COMPARISON_FUNC_LESS_EQUAL : D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.MinLOD         = desc.MinLod;
		samplerDesc.MaxLOD         = desc.MaxLod;

		// Allocate from heap at slot 0 (single sampler for now)
		m_CPUHandle = samplerHeap->GetCPUDescriptorHandleForHeapStart();
		m_GPUHandle = samplerHeap->GetGPUDescriptorHandleForHeapStart();

		device->CreateSampler(&samplerDesc, m_CPUHandle);
	}

	D3D12Sampler::~D3D12Sampler() = default;

} // namespace Candy
