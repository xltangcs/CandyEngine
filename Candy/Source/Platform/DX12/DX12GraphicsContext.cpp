#include "CandyPCH.h"
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "Platform/DX12/DX12GraphicsContext.h"
#include "Platform/DX12/DX12Device.h"
#include "Platform/DX12/DX12SwapChain.h"
#include "Runtime/Core/Log.h"
#include "Runtime/RHI/RHISwapChain.h"

namespace Candy {

	DX12GraphicsContext::DX12GraphicsContext(const WindowHandle& handle)
		: m_Window(static_cast<GLFWwindow*>(handle.Native))
	{
		CANDY_CORE_ASSERT(m_Window, "DX12GraphicsContext: null window handle");
	}

	DX12GraphicsContext::~DX12GraphicsContext()
	{
		CANDY_CORE_INFO("DX12GraphicsContext: shutting down");
		m_SwapChain.reset();
		m_Device.reset();
	}

	void DX12GraphicsContext::Init()
	{
		CANDY_CORE_INFO("DX12GraphicsContext: initializing...");

		// Create DX12 device
		m_Device = std::make_unique<DX12Device>();
		if (!m_Device->GetNativeDevice())
		{
			CANDY_CORE_ERROR("DX12GraphicsContext: DX12Device creation failed");
			return;
		}

		HWND hwnd = glfwGetWin32Window(m_Window);
		if (!hwnd)
		{
			CANDY_CORE_ERROR("DX12GraphicsContext: failed to get HWND from GLFW window");
			return;
		}

		int width = 0, height = 0;
		glfwGetFramebufferSize(m_Window, &width, &height);
		if (width <= 0 || height <= 0) { width = 1280; height = 720; }

		SwapChainDesc scDesc;
		scDesc.Window      = WindowHandle{ hwnd };
		scDesc.Width       = static_cast<uint32_t>(width);
		scDesc.Height      = static_cast<uint32_t>(height);
		scDesc.BufferCount = 2;
		scDesc.VSync       = true;

		auto sc = m_Device->CreateSwapChain(scDesc);
		if (!sc)
		{
			CANDY_CORE_ERROR("DX12GraphicsContext: SwapChain creation failed");
			return;
		}

		// Store swap chain as Ref<> (shared_ptr), cast as needed
		m_SwapChainRef = sc;

		CANDY_CORE_INFO("DX12GraphicsContext: initialized ({}x{})", width, height);
	}

	void DX12GraphicsContext::SwapBuffers()
	{
		// DX12 presents explicitly via the command queue during the render
		// pass.  This hook is a no-op; the platform window's OnUpdate calls
		// SwapBuffers after all layers have finished rendering, but in DX12
		// mode the presentation has already happened.
	}

} // namespace Candy
