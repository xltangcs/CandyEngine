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

		[[nodiscard]] D3D12Device*     GetDevice()     const { return m_Device.get(); }
		[[nodiscard]] D3D12SwapChain*  GetSwapChain()  const { return reinterpret_cast<D3D12SwapChain*>(m_SwapChainRef.get()); }

	private:
		GLFWwindow*                 m_Window = nullptr;
		std::unique_ptr<D3D12Device> m_Device;
		Ref<RHISwapChain>           m_SwapChainRef;
	};

} // namespace Candy
