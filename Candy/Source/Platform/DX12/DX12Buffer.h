#pragma once

#include "Runtime/RHI/RHIDevice.h"  // RHIBuffer, BufferDesc

#include <d3d12.h>
#include <wrl/client.h>

namespace Candy {

	// =========================================================================
	// DX12Buffer — wraps ID3D12Resource for vertex/index/constant/storage
	// =========================================================================
	class DX12Buffer : public RHIBuffer
	{
	public:
		DX12Buffer(ID3D12Device* device, const BufferDesc& desc);
		virtual ~DX12Buffer();

		const BufferDesc& GetDesc() const override;

		void* Map() override;
		void  Unmap() override;

		[[nodiscard]] ID3D12Resource*         GetResource()       { return m_Resource.Get(); }
		[[nodiscard]] D3D12_RESOURCE_STATES   GetState()    const { return m_State; }
		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return m_GPUVirtualAddress; }

		/// Transition the resource to a new state (for barrier tracking)
		void SetState(D3D12_RESOURCE_STATES state) { m_State = state; }

	private:
		void CreateUploadBuffer(ID3D12Device* device);
		void CreateDefaultBuffer(ID3D12Device* device);

		BufferDesc                     m_Desc;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_Resource;
		D3D12_RESOURCE_STATES          m_State              = D3D12_RESOURCE_STATE_COMMON;
		D3D12_GPU_VIRTUAL_ADDRESS      m_GPUVirtualAddress  = 0;
		void*                          m_MappedData          = nullptr;
		bool                           m_IsCPUMapped         = false;
	};

} // namespace Candy
