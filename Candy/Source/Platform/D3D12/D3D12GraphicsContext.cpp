#include "CandyPCH.h"
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "Platform/D3D12/D3D12GraphicsContext.h"
#include "Platform/D3D12/D3D12Device.h"
#include "Platform/D3D12/D3D12SwapChain.h"
#include "Runtime/Core/Log.h"
#include "Runtime/RHI/RHISwapChain.h"

namespace Candy {

	D3D12GraphicsContext::D3D12GraphicsContext(const WindowHandle& handle)
		: m_Window(static_cast<GLFWwindow*>(handle.Native))
	{
		CANDY_CORE_ASSERT(m_Window, "D3D12GraphicsContext: null window handle");
	}

	D3D12GraphicsContext::~D3D12GraphicsContext()
	{
		CANDY_CORE_INFO("D3D12GraphicsContext: shutting down");
		m_SwapChainRef.reset();
		m_Device.reset();
	}

	void D3D12GraphicsContext::Init()
	{
		CANDY_CORE_INFO("D3D12GraphicsContext: initializing...");

		// Create D3D12 device
		m_Device = std::make_unique<D3D12Device>();
		if (!m_Device->GetNativeDevice())
		{
			CANDY_CORE_ERROR("D3D12GraphicsContext: D3D12Device creation failed");
			return;
		}

		HWND hwnd = glfwGetWin32Window(m_Window);
		if (!hwnd)
		{
			CANDY_CORE_ERROR("D3D12GraphicsContext: failed to get HWND from GLFW window");
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
			CANDY_CORE_ERROR("D3D12GraphicsContext: SwapChain creation failed");
			return;
		}

		// Store swap chain as Ref<> (shared_ptr), cast as needed
		m_SwapChainRef = sc;

		CANDY_CORE_INFO("D3D12GraphicsContext: initialized ({}x{})", width, height);
	}

	void D3D12GraphicsContext::SwapBuffers()
	{
		// D3D12 presents explicitly via the command queue during the render
		// pass.  This hook is a no-op; the platform window's OnUpdate calls
		// SwapBuffers after all layers have finished rendering, but in D3D12
		// mode the presentation has already happened.
	}

} // namespace Candy
