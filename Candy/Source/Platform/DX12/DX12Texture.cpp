#include <Windows.h>
#include <d3d12.h>

#include "Platform/DX12/DX12Texture.h"
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
		case SamplerAddressMode::MirrorRepeat:  return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		default:                                return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		}
	}

	// =========================================================================
	// DX12Texture
	// =========================================================================

	DX12Texture::DX12Texture(ID3D12Device* device, const TextureDesc& desc)
		: m_Desc(desc), m_Device(device)
	{
		DXGI_FORMAT dxgiFormat = MapRHIFormatToDXGI(desc.Format);
		if (dxgiFormat == DXGI_FORMAT_UNKNOWN)
		{
			CANDY_CORE_ERROR("DX12Texture: unsupported format");
			return;
		}

		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resDesc.Width            = desc.Width;
		resDesc.Height           = desc.Height;
		resDesc.DepthOrArraySize = static_cast<UINT16>(desc.Depth);
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

		HRESULT hr = device->CreateCommittedResource(
			&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
			D3D12_RESOURCE_STATE_COMMON, pClearValue,
			IID_PPV_ARGS(&m_Resource));

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("DX12Texture: CreateCommittedResource failed for {}x{} texture",
			                 desc.Width, desc.Height);
			return;
		}

		m_State = D3D12_RESOURCE_STATE_COMMON;

		CANDY_CORE_INFO("DX12Texture: created {}x{} (format: {}, mips: {})",
		                desc.Width, desc.Height, static_cast<int>(desc.Format), desc.MipLevels);
	}

	DX12Texture::~DX12Texture()
	{
		m_Resource.Reset();
	}

	void DX12Texture::SetData(const void* data, uint32_t rowPitch, uint32_t slicePitch)
	{
		if (!data || !m_Resource || !m_Device)
			return;

		uint64_t totalSize;
		{
			uint64_t rowSize = rowPitch > 0 ? rowPitch : m_Desc.Width * 4; // assume RGBA8
			uint64_t numRows = m_Desc.Height;
			totalSize = rowSize * numRows;
			totalSize = (totalSize + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1)
			          & ~static_cast<uint64_t>(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);
		}

		// Create upload buffer
		D3D12_HEAP_PROPERTIES uploadHeapProps = {};
		uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC uploadDesc = {};
		uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		uploadDesc.Width     = totalSize;
		uploadDesc.Height    = 1;
		uploadDesc.DepthOrArraySize = 1;
		uploadDesc.MipLevels = 1;
		uploadDesc.Format    = DXGI_FORMAT_UNKNOWN;
		uploadDesc.Layout    = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		uploadDesc.SampleDesc.Count = 1;

		Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
		HRESULT hr = m_Device->CreateCommittedResource(
			&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&uploadBuffer));

		if (FAILED(hr))
		{
			CANDY_CORE_ERROR("DX12Texture::SetData: upload buffer creation failed");
			return;
		}

		// Copy data
		void* mapped = nullptr;
		D3D12_RANGE readRange = { 0, 0 };
		uploadBuffer->Map(0, &readRange, &mapped);
		if (mapped)
		{
			uint32_t srcRowPitch = rowPitch > 0 ? rowPitch : m_Desc.Width * 4;
			uint32_t dstRowPitchAligned = static_cast<uint32_t>(
				(totalSize + m_Desc.Height - 1) / m_Desc.Height);
			for (uint32_t row = 0; row < m_Desc.Height; ++row)
			{
				memcpy(static_cast<uint8_t*>(mapped) + row * dstRowPitchAligned,
				       static_cast<const uint8_t*>(data) + row * srcRowPitch,
				       srcRowPitch);
			}
			uploadBuffer->Unmap(0, nullptr);
		}

		// Copy upload → texture using temp command list
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> tempAllocator;
		hr = m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAllocator));
		if (FAILED(hr)) return;

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> tempCmdList;
		hr = m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
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
		srcLoc.PlacedFootprint.Offset = 0;

		DXGI_FORMAT dxgiFormat = MapRHIFormatToDXGI(m_Desc.Format);
		m_Device->GetCopyableFootprints(
			&CD3DX12_RESOURCE_DESC(m_Resource->GetDesc()), 0, 1, 0,
			&srcLoc.PlacedFootprint, nullptr, nullptr, nullptr);

		tempCmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

		// Transition texture to PIXEL_SHADER_RESOURCE
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		tempCmdList->ResourceBarrier(1, &barrier);

		tempCmdList->Close();

		m_State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}

	void DX12Texture::CreateSRV(ID3D12DescriptorHeap* heap, uint32_t slotIndex, uint32_t descriptorSize) const
	{
		if (!m_Resource || !m_Device || !heap)
			return;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format                  = MapRHIFormatToDXGI(m_Desc.Format);
		srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels     = m_Desc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = heap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += static_cast<SIZE_T>(slotIndex) * descriptorSize;

		m_Device->CreateShaderResourceView(m_Resource.Get(), &srvDesc, cpuHandle);
	}

	// =========================================================================
	// DX12Sampler
	// =========================================================================

	DX12Sampler::DX12Sampler(ID3D12Device* device, ID3D12DescriptorHeap* samplerHeap,
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

	DX12Sampler::~DX12Sampler() = default;

} // namespace Candy
