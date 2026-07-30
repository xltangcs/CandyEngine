#pragma once

#include "Runtime/Renderer/Framebuffer.h"
#include "Runtime/RHI/RHIFramebuffer.h"

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
	class D3D12Framebuffer : public Framebuffer, public RHIFramebuffer
	{
	public:
		D3D12Framebuffer(const FramebufferSpecification& spec, D3D12Device* device);
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

		const FramebufferSpecification& GetSpecification() const override { return m_Specification; }

		bool IsSwapChainTarget() const { return m_Specification.SwapChainTarget; }

		// ---- RHIFramebuffer bridge (Runtime code reaches through these) ----

		const FramebufferDesc& GetDesc() const override { return m_RHIDesc; }
		uint32_t GetWidth()                 const override { return m_Specification.Width;  }
		uint32_t GetHeight()                const override { return m_Specification.Height; }
		uint32_t GetColorAttachmentCount()  const override { return static_cast<uint32_t>(m_ColorAttachments.size()); }
		bool     HasDepthStencil()          const override { return HasDepthAttachment(); }

		// ---- D3D12-specific accessors for command buffer integration --------

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(uint32_t index = 0) const;
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const;
		[[nodiscard]] bool     HasDepthAttachment() const;

		/// Returns the color SRV GPU descriptor handle for ImGui display.
		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetColorSRVGPUHandle(uint32_t index = 0) const;

	private:
		void Invalidate();
		void CreateColorTexture(uint32_t index, FramebufferTextureFormat format);
		void CreateDepthTexture();

		DXGI_FORMAT MapFormat(FramebufferTextureFormat format) const;

		FramebufferSpecification m_Specification;
		D3D12Device*              m_Device = nullptr;

		// Attachment resources
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_ColorAttachments;
		Microsoft::WRL::ComPtr<ID3D12Resource>              m_DepthAttachment;

		std::vector<FramebufferTextureSpecification> m_ColorAttachmentSpecs;
		FramebufferTextureSpecification              m_DepthAttachmentSpec = FramebufferTextureFormat::None;

		// Descriptor heaps
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DSVHeap;
		uint32_t m_RTVDescriptorSize = 0;
		uint32_t m_DSVDescriptorSize = 0;

		// SRV GPU handles (allocated from device CBV_SRV_UAV heap on each Invalidate)
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> m_ColorSRVGPUHandles;

		// Readback buffer for ReadPixel
		Microsoft::WRL::ComPtr<ID3D12Resource> m_ReadbackBuffer;
		uint64_t m_ReadbackBufferSize = 0;

		// RHI bridge description kept in sync inside Invalidate/Resize.
		FramebufferDesc m_RHIDesc;
	};

} // namespace Candy
