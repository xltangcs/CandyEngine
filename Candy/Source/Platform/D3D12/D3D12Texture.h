#pragma once

#include "Runtime/RHI/RHIDevice.h"

#include <d3d12.h>
#include <wrl/client.h>

namespace Candy {

	class D3D12Device;

	// =========================================================================
	// D3D12Texture — wraps ID3D12Resource for a 2D texture
	// =========================================================================
	class D3D12Texture : public RHITexture
	{
	public:
		/// `device` must be the D3D12Device (owns the queue + fence used to upload).
		D3D12Texture(D3D12Device* device, const TextureDesc& desc);
		virtual ~D3D12Texture();

		const TextureDesc& GetDesc() const override { return m_Desc; }

		/// Upload pixel data to the texture (creates upload buffer, copies, transitions,
		/// executes the copy on the device queue and waits for it to finish).
		void SetData(const void* data, uint32_t rowPitch, uint32_t slicePitch = 0);

		/// Upload mip × slice subresources in one copy (cubemaps, mip chains).
		bool WriteSubresources(const TextureSubresourceData* data, uint32_t count) override;

		[[nodiscard]] ID3D12Resource* GetResource() const { return m_Resource.Get(); }
		[[nodiscard]] D3D12_RESOURCE_STATES GetState() const { return m_State; }
		void SetState(D3D12_RESOURCE_STATES state) { m_State = state; }

		/// Write SRV descriptor into the device CBV_SRV_UAV heap at the given slot.
		void CreateSRV(ID3D12DescriptorHeap* heap, uint32_t slotIndex, uint32_t descriptorSize) const;

		/// Write a UAV descriptor into the device CBV_SRV_UAV heap at the given
		/// slot. `mipSlice` selects the mip level (cubemaps map to a
		/// Texture2DArray UAV over the 6 faces).
		void CreateUAV(ID3D12DescriptorHeap* heap, uint32_t slotIndex, uint32_t descriptorSize,
		               uint32_t mipSlice = 0) const;

		/// Insert a transition barrier for the whole resource on `list` and
		/// update the tracked state.
		void Transition(ID3D12GraphicsCommandList* list, D3D12_RESOURCE_STATES to);
		/// Insert a transition barrier for one subresource (mip*arrayLayer).
		/// The caller tracks per-subresource states (bake pipelines); the
		/// whole-resource tracked state is left untouched.
		void TransitionSubresource(ID3D12GraphicsCommandList* list, uint32_t subresource,
		                           D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);
		/// Insert a UAV barrier for this resource (dispatch write visibility).
		void UAVBarrier(ID3D12GraphicsCommandList* list) const;

	private:
		TextureDesc                           m_Desc;
		D3D12Device*                          m_Device = nullptr;
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
