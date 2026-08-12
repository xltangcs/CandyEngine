#pragma once

#include "Runtime/Renderer/GraphicsContext.h"
#include "Runtime/Core/Base.h"
#include <memory>

struct GLFWwindow;

namespace Candy {

	class D3D12Device;
	class D3D12SwapChain;
	class RHISwapChain;

	// =========================================================================
	// D3D12GraphicsContext — owns D3D12Device + D3D12SwapChain for the main window
	// =========================================================================
	class D3D12GraphicsContext : public GraphicsContext
	{
	public:
		D3D12GraphicsContext(const WindowHandle& handle);
		virtual ~D3D12GraphicsContext();

		void Init() override;
		void SwapBuffers() override;
		void OnResize(uint32_t width, uint32_t height) override;

		/// Resize the swap chain to the given size (drains the GPU first).
		void ResizeSwapChain(uint32_t width, uint32_t height);

		/// If the window's current framebuffer size differs from the swap chain
		/// size, resize the swap chain to match. Call before Present so a flip-model
		/// swap chain never presents a stale-size back buffer (which removes the
		/// GPU with DXGI_ERROR_DEVICE_HUNG).
		void EnsureSwapChainMatchesWindow();

		[[nodiscard]] D3D12Device*     GetDevice()     const { return m_Device.get(); }
		[[nodiscard]] D3D12SwapChain*  GetSwapChain()  const { return reinterpret_cast<D3D12SwapChain*>(m_SwapChainRef.get()); }

	private:
		GLFWwindow*                 m_Window = nullptr;
		std::unique_ptr<D3D12Device> m_Device;
		Ref<RHISwapChain>           m_SwapChainRef;
	};

} // namespace Candy
