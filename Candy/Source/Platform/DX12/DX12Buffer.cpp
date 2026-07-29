#include <Windows.h>
#include <d3d12.h>

#include "Platform/DX12/DX12Buffer.h"
#include "Runtime/Core/Log.h"

namespace Candy {

	DX12Buffer::DX12Buffer(ID3D12Device* device, const BufferDesc& desc)
		: m_Desc(desc)
	{
		if (desc.CPUAccessible)
			CreateUploadBuffer(device);
		else
			CreateDefaultBuffer(device);

		m_GPUVirtualAddress = m_Resource->GetGPUVirtualAddress();

		CANDY_CORE_TRACE("DX12Buffer: created '{}' ({} bytes, CPUAccessible={})",
		                 desc.DebugName, desc.Size, desc.CPUAccessible);
	}

	DX12Buffer::~DX12Buffer()
	{
		if (m_IsCPUMapped)
			Unmap();
	}

	const BufferDesc& DX12Buffer::GetDesc() const
	{
		return m_Desc;
	}

	void* DX12Buffer::Map()
	{
		if (!m_IsCPUMapped && m_Resource)
		{
			D3D12_RANGE readRange = { 0, 0 }; // We're writing, not reading back
			HRESULT hr = m_Resource->Map(0, &readRange, &m_MappedData);
			if (SUCCEEDED(hr))
				m_IsCPUMapped = true;
			else
				CANDY_CORE_ERROR("DX12Buffer::Map failed for '{}'", m_Desc.DebugName);
		}
		return m_MappedData;
	}

	void DX12Buffer::Unmap()
	{
		if (m_IsCPUMapped && m_Resource)
		{
			m_Resource->Unmap(0, nullptr);
			m_MappedData  = nullptr;
			m_IsCPUMapped = false;
		}
	}

	void DX12Buffer::CreateUploadBuffer(ID3D12Device* device)
	{
		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type                 = D3D12_HEAP_TYPE_UPLOAD;
		heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width              = m_Desc.Size;
		resourceDesc.Height             = 1;
		resourceDesc.DepthOrArraySize   = 1;
		resourceDesc.MipLevels          = 1;
		resourceDesc.Format             = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count   = 1;
		resourceDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

		HRESULT hr = device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_Resource));

		if (FAILED(hr))
			CANDY_CORE_ERROR("DX12Buffer: CreateCommittedResource (upload) failed for '{}'", m_Desc.DebugName);
		else
			m_State = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	void DX12Buffer::CreateDefaultBuffer(ID3D12Device* device)
	{
		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type                 = D3D12_HEAP_TYPE_DEFAULT;
		heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
		if (HasFlag(m_Desc.Usage, ResourceUsage::ShaderWrite))
			resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width              = m_Desc.Size;
		resourceDesc.Height             = 1;
		resourceDesc.DepthOrArraySize   = 1;
		resourceDesc.MipLevels          = 1;
		resourceDesc.Format             = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count   = 1;
		resourceDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags              = resourceFlags;

		HRESULT hr = device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&m_Resource));

		if (FAILED(hr))
			CANDY_CORE_ERROR("DX12Buffer: CreateCommittedResource (default) failed for '{}'", m_Desc.DebugName);
		else
			m_State = D3D12_RESOURCE_STATE_COMMON;
	}

} // namespace Candy
