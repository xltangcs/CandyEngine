#pragma once

#include "Runtime/Renderer/Framebuffer.h"
#include "Platform/D3D12/D3D12Texture.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>

namespace Candy {

	class D3D12Device;

	// =========================================================================
	// D3D12Framebuffer — off-screen render target for viewport / PIP
	//
	// Creates committed resources for color (RGBA8 + RED_INTEGER) and depth
	// (D24S8) attachments.  Manages RTV/DSV descriptor heaps and creates SRVs
	// in the device's shared CBV_SRV_UAV heap so ImGui_ImplDX12 can display
	// the color attachment.
	// =========================================================================
	class D3D12Framebuffer : public Framebuffer
	{
	public:
		D3D12Framebuffer(const FramebufferDesc& desc, D3D12Device* device);
		virtual ~D3D12Framebuffer();

		void Bind() override;
		void Unbind() override;

		void Resize(uint32_t width, uint32_t height) override;
		int  ReadPixel(uint32_t attachmentIndex, int x, int y) override;
		void ClearAttachment(uint32_t attachmentIndex, int value) override;

		/// Returns GPU descriptor handle .ptr (lower 32 bits, for legacy compat).
		uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const override;

		/// Returns full 64-bit GPU descriptor handle .ptr for ImGui::Image.
		uint64_t GetColorAttachmentGPUHandle(uint32_t index = 0) const override;

		/// Returns the color attachment as a sampled texture (adopting
		/// wrapper around the attachment resource; state is kept in sync with
		/// EnsureColorAttachmentState).
		Ref<RHITexture> GetColorAttachmentTexture(uint32_t index = 0) override;

		bool IsSwapChainTarget() const { return m_Desc.SwapChainTarget; }

		// ---- RHIFramebuffer (via Framebuffer) ----------------------------

		const FramebufferDesc& GetDesc() const override { return m_Desc; }
		uint32_t GetWidth()                 const override { return m_Desc.Width;  }
		uint32_t GetHeight()                const override { return m_Desc.Height; }
		uint32_t GetColorAttachmentCount()  const override { return static_cast<uint32_t>(m_ColorAttachments.size()); }
		bool     HasDepthStencil()          const override { return m_Desc.HasDepthStencil; }

		// ---- D3D12-specific accessors for command buffer integration --------

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(uint32_t index = 0) const;
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const;
		[[nodiscard]] bool     HasDepthAttachment() const;

		/// Returns the color SRV GPU descriptor handle for ImGui display.
		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetColorSRVGPUHandle(uint32_t index = 0) const;

		/// Transition a color attachment to `target` (inserts a barrier if needed).
		/// Used by D3D12CommandBuffer to switch the framebuffer between the game
		/// render pass (RENDER_TARGET) and the editor ImGui pass (PIXEL_SHADER_RESOURCE).
		void EnsureColorAttachmentState(ID3D12GraphicsCommandList* list, uint32_t index, D3D12_RESOURCE_STATES target);
		/// Record the state of a color attachment without a barrier (used by ReadPixel).
		void SetColorAttachmentState(uint32_t index, D3D12_RESOURCE_STATES state);

	private:
		void Invalidate();
		void CreateColorTexture(uint32_t index, RHIFormat format);
		void CreateDepthTexture();

		DXGI_FORMAT MapFormat(RHIFormat format) const;

		FramebufferDesc           m_Desc;
		D3D12Device*              m_Device = nullptr;

		// Attachment resources
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_ColorAttachments;
		Microsoft::WRL::ComPtr<ID3D12Resource>              m_DepthAttachment;

		// Non-owning sampled-texture wrappers for the color attachments
		// (rebuilt on Invalidate; used by the tonemap pass).
		std::vector<Ref<D3D12Texture>> m_ColorAttachmentTextures;

		// Descriptor heaps
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DSVHeap;
		uint32_t m_RTVDescriptorSize = 0;
		uint32_t m_DSVDescriptorSize = 0;

		// SRV GPU handles (allocated from device CBV_SRV_UAV heap on each Invalidate)
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> m_ColorSRVGPUHandles;

		// Current D3D12 resource state of each color attachment. Used to insert
		// the correct resource barriers between the game render pass (RENDER_TARGET)
		// and the editor ImGui pass (PIXEL_SHADER_RESOURCE for the viewport image).
		std::vector<D3D12_RESOURCE_STATES> m_ColorAttachmentStates;

		// Base SRV descriptor slot for THIS framebuffer (unique per instance,
		// handed out by the device's IR descriptor range allocator, so the main
		// viewport framebuffer and the camera-preview PIP do not stomp each
		// other's descriptors).
		uint32_t m_SRVBaseSlot = 0;

		// Readback buffer for ReadPixel
		Microsoft::WRL::ComPtr<ID3D12Resource> m_ReadbackBuffer;
		uint64_t m_ReadbackBufferSize = 0;
	};

} // namespace Candy
