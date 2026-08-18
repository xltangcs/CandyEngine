#include "CandyPCH.h"
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "Platform/D3D12/D3D12GraphicsContext.h"
#include "Platform/D3D12/D3D12Device.h"
#include "Platform/D3D12/D3D12SwapChain.h"
#include "Runtime/Core/Log.h"
#include "Runtime/RHI/RHISwapChain.h"
#include "Runtime/RHI/RHIContext.h"

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

		// Publish to the process-wide RHI registry so renderers / Editor
		// code can reach the active device without including Platform headers.
		RHIContext::SetDevice(m_Device.get());
		RHIContext::SetSwapChain(m_SwapChainRef.get());

		CANDY_CORE_INFO("D3D12GraphicsContext: initialized ({}x{})", width, height);
	}

	void D3D12GraphicsContext::SwapBuffers()
	{
		// D3D12 presents explicitly via the command queue during the render
		// pass.  This hook is a no-op; the platform window's OnUpdate calls
		// SwapBuffers after all layers have finished rendering, but in D3D12
		// mode the presentation has already happened.
	}

	void D3D12GraphicsContext::ResizeSwapChain(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
			return;
		if (!m_SwapChainRef)
			return;

		// Drain the queue before ResizeBuffers. The GPU may still be executing
		// the just-presented frame (ImGui presents on the same native queue),
		// and calling ResizeBuffers while a present is in flight is a classic
		// DXGI_ERROR_DEVICE_REMOVED (0x887A0005) trigger.
		if (m_Device)
			m_Device->WaitIdle();

		m_SwapChainRef->Resize(width, height);
	}

	void D3D12GraphicsContext::OnResize(uint32_t width, uint32_t height)
	{
		ResizeSwapChain(width, height);
	}

	void D3D12GraphicsContext::EnsureSwapChainMatchesWindow()
	{
		if (!m_Window || !m_SwapChainRef)
			return;

		int fbW = 0, fbH = 0;
		glfwGetFramebufferSize(m_Window, &fbW, &fbH);
		if (fbW <= 0 || fbH <= 0)
			return; // minimized

		uint32_t w = static_cast<uint32_t>(fbW);
		uint32_t h = static_cast<uint32_t>(fbH);
		if (m_SwapChainRef->GetWidth() == w && m_SwapChainRef->GetHeight() == h)
			return;

		// The window was resized after the last event poll but before this
		// frame's Present. Flip-model swapchains REQUIRE the back buffer size to
		// match the window — presenting a stale-size buffer hangs the display
		// pipeline and the GPU is removed with DXGI_ERROR_DEVICE_HUNG (TDR).
		CANDY_CORE_INFO("D3D12GraphicsContext: swap chain out of sync with window ({})x({} -> {}x{})",
		                m_SwapChainRef->GetWidth(), m_SwapChainRef->GetHeight(), w, h);
		ResizeSwapChain(w, h);
	}

} // namespace Candy
