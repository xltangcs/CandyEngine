#pragma once

#include "Runtime/Renderer/GraphicsContext.h"
#include "Runtime/Core/Base.h"
#include <memory>

struct GLFWwindow;

namespace Candy {

	class DX12Device;
	class DX12SwapChain;

	// =========================================================================
	// DX12GraphicsContext — owns DX12Device + DX12SwapChain for the main window
	// =========================================================================
	class DX12GraphicsContext : public GraphicsContext
	{
	public:
		DX12GraphicsContext(const WindowHandle& handle);
		virtual ~DX12GraphicsContext();

		void Init() override;
		void SwapBuffers() override;

		[[nodiscard]] DX12Device*     GetDevice()     const { return m_Device.get(); }
		[[nodiscard]] DX12SwapChain*  GetSwapChain()  const { return static_cast<DX12SwapChain*>(m_SwapChainRef.get()); }

	private:
		GLFWwindow*                 m_Window = nullptr;
		std::unique_ptr<DX12Device> m_Device;
		Ref<RHISwapChain>           m_SwapChainRef;
	};

} // namespace Candy
