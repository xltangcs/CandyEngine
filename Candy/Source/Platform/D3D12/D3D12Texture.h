#pragma once

#include "Runtime/RHI/RHIDevice.h"

#include <d3d12.h>
#include <wrl/client.h>

namespace Candy {

	// =========================================================================
	// D3D12Texture — wraps ID3D12Resource for a 2D texture
	// =========================================================================
	class D3D12Texture : public RHITexture
	{
	public:
		D3D12Texture(ID3D12Device* device, const TextureDesc& desc);
		virtual ~D3D12Texture();

		const TextureDesc& GetDesc() const override { return m_Desc; }

		/// Upload pixel data to the texture (creates upload buffer, copies, transitions).
		void SetData(const void* data, uint32_t rowPitch, uint32_t slicePitch = 0);

		[[nodiscard]] ID3D12Resource* GetResource() const { return m_Resource.Get(); }
		[[nodiscard]] D3D12_RESOURCE_STATES GetState() const { return m_State; }
		void SetState(D3D12_RESOURCE_STATES state) { m_State = state; }

		/// Write SRV descriptor into the device CBV_SRV_UAV heap at the given slot.
		void CreateSRV(ID3D12DescriptorHeap* heap, uint32_t slotIndex, uint32_t descriptorSize) const;

	private:
		TextureDesc                           m_Desc;
		ID3D12Device*                         m_Device = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_Resource;
		D3D12_RESOURCE_STATES                 m_State = D3D12_RESOURCE_STATE_COMMON;
	};

	// =========================================================================
	// D3D12Sampler — wraps a sampler descriptor in the device sampler heap
	// =========================================================================
	class D3D12Sampler : public RHISampler
	{
	public:
		D3D12Sampler(ID3D12Device* device, ID3D12DescriptorHeap* samplerHeap,
		            uint32_t descriptorSize, const SamplerDesc& desc);
		virtual ~D3D12Sampler();

		const SamplerDesc& GetDesc() const override { return m_Desc; }

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle() const { return m_CPUHandle; }
		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle() const { return m_GPUHandle; }

	private:
		SamplerDesc              m_Desc;
		ID3D12Device*            m_Device = nullptr;
		ID3D12DescriptorHeap*    m_SamplerHeap = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE m_CPUHandle = {};
		D3D12_GPU_DESCRIPTOR_HANDLE m_GPUHandle = {};
	};

} // namespace Candy
