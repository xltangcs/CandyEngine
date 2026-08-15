#include "CandyPCH.h"
#include <Windows.h>

#include "Platform/D3D12/D3D12ImGuiBackend.h"

#include "Runtime/Core/Application.h"
#include "Runtime/Core/Log.h"
#include "Platform/D3D12/D3D12GraphicsContext.h"
#include "Platform/D3D12/D3D12Device.h"
#include "Platform/D3D12/D3D12SwapChain.h"
#include "Platform/D3D12/D3D12Framebuffer.h"
#include "Platform/Windows/WindowsWindow.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_dx12.h>

namespace Candy {

	// =========================================================================
	// SRV descriptor allocation (shared device heap; per-context regions)
	// =========================================================================

	void D3D12ImGuiBackend::SRVAllocator(ImGui_ImplDX12_InitInfo* info,
	                                     D3D12_CPU_DESCRIPTOR_HANDLE* outCPU,
	                                     D3D12_GPU_DESCRIPTOR_HANDLE* outGPU)
	{
		auto* self = static_cast<D3D12ImGuiBackend*>(info->UserData);

		// Each ImGui context owns a distinct descriptor region of the device
		// heap so their fonts never collide; both region bases were handed out
		// by the device's IR descriptor range allocator at Init. The per-context
		// counters persist (fonts are created once and stay put).
		const bool isGameUI = (ImGui::GetCurrentContext() == self->m_GameUIContext);
		const uint32_t kBase = isGameUI ? self->m_GameUISRVBase : self->m_EditorSRVBase;
		uint32_t& used = isGameUI ? self->m_SRVHeapUsedGameUI : self->m_SRVHeapUsedEditor;

		D3D12_CPU_DESCRIPTOR_HANDLE cpu = self->m_SRVHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE gpu = self->m_SRVHeap->GetGPUDescriptorHandleForHeapStart();

		cpu.ptr += static_cast<SIZE_T>(kBase + used) * self->m_SRVDescSize;
		gpu.ptr += static_cast<SIZE_T>(kBase + used) * self->m_SRVDescSize;

		used++;

		*outCPU = cpu;
		*outGPU = gpu;
	}

	void D3D12ImGuiBackend::SRVDeallocator(ImGui_ImplDX12_InitInfo* /*info*/,
	                                       D3D12_CPU_DESCRIPTOR_HANDLE /*cpu*/,
	                                       D3D12_GPU_DESCRIPTOR_HANDLE /*gpu*/)
	{
		// ImGui_ImplDX12_InitInfo requires both Alloc and Free callbacks to be
		// non-null (see assertion in ImGui_ImplDX12_Init). Our allocator is a
		// simple linear bump, so nothing to release.
	}

	// =========================================================================
	// Init / Shutdown
	// =========================================================================

	void D3D12ImGuiBackend::Init(GLFWwindow* window)
	{
		// GLFW platform backend (no OpenGL context)
		ImGui_ImplGlfw_InitForOther(window, true);

		auto* win    = static_cast<WindowsWindow*>(&Application::Get().GetWindow());
		auto* gfxCtx = static_cast<D3D12GraphicsContext*>(win->GetGraphicsContext());

		m_Device = gfxCtx->GetDevice()->GetNativeDevice();
		m_Queue  = gfxCtx->GetDevice()->GetNativeQueue();

		// SRV descriptor heap for ImGui textures. Use the *device's* shared
		// CBV_SRV_UAV heap so that ALL SRVs ImGui displays (fonts, viewport
		// framebuffer, engine icons/textures) live in ONE heap that the render
		// pass binds. D3D12 allows only one CBV_SRV_UAV heap per
		// SetDescriptorHeaps, so a separate small heap would leave the viewport
		// / icon descriptors out of the bound heap (they would not display).
		m_SRVHeap     = gfxCtx->GetDevice()->GetCBVSRVUAVHeap();
		m_SRVDescSize = gfxCtx->GetDevice()->GetCBVSRVDescriptorSize();
		if (!m_SRVHeap)
		{
			CANDY_CORE_ERROR("D3D12ImGuiBackend::Init: no device CBV_SRV_UAV heap");
			return;
		}

		// Each ImGui context gets its own 32-slot descriptor region from the
		// device's IR descriptor range allocator (fonts + per-frame images).
		m_EditorSRVBase  = gfxCtx->GetDevice()->AllocateSRVRange(32);
		m_GameUISRVBase  = gfxCtx->GetDevice()->AllocateSRVRange(32);

		// Per-frame command lists (shared across both contexts — they iterate
		// sequentially per main loop so they do not stomp each other).
		for (int i = 0; i < 2; ++i)
		{
			HRESULT hr = m_Device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(&m_FrameAllocators[i]));
			if (FAILED(hr))
			{
				CANDY_CORE_ERROR("D3D12ImGuiBackend: CreateCommandAllocator[{0}] failed", i);
				return;
			}

			hr = m_Device->CreateCommandList(
				0, D3D12_COMMAND_LIST_TYPE_DIRECT,
				m_FrameAllocators[i], nullptr,
				IID_PPV_ARGS(&m_FrameCmdLists[i]));
			if (FAILED(hr))
			{
				CANDY_CORE_ERROR("D3D12ImGuiBackend: CreateCommandList[{0}] failed", i);
				return;
			}

			m_FrameCmdLists[i]->Close();
		}

		m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
		m_FenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);

		// Dedicated game UI allocator/list (renders to the viewport framebuffer,
		// separate from the editor's swap-chain-bound frame state).
		m_Device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&m_GameUIAllocator));
		m_Device->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_GameUIAllocator, nullptr,
			IID_PPV_ARGS(&m_GameUICmdList));
		if (m_GameUICmdList)
			m_GameUICmdList->Close();
	}

	void D3D12ImGuiBackend::InitContext()
	{
		// First call binds the editor context, second the game UI context.
		if (!m_EditorContext)
			m_EditorContext = ImGui::GetCurrentContext();
		else if (!m_GameUIContext)
			m_GameUIContext = ImGui::GetCurrentContext();

		ImGui_ImplDX12_InitInfo initInfo = {};
		initInfo.Device               = m_Device;
		initInfo.CommandQueue         = m_Queue;
		initInfo.NumFramesInFlight    = 2;
		initInfo.RTVFormat            = DXGI_FORMAT_B8G8R8A8_UNORM;
		initInfo.DSVFormat            = DXGI_FORMAT_UNKNOWN;
		initInfo.SrvDescriptorHeap    = m_SRVHeap;
		initInfo.SrvDescriptorAllocFn = SRVAllocator;
		initInfo.SrvDescriptorFreeFn  = SRVDeallocator;
		initInfo.UserData             = this;

		if (!ImGui_ImplDX12_Init(&initInfo))
			CANDY_CORE_ERROR("D3D12ImGuiBackend: ImGui_ImplDX12_Init failed");
	}

	void D3D12ImGuiBackend::ShutdownContext()
	{
		ImGui_ImplDX12_Shutdown();
	}

	void D3D12ImGuiBackend::Shutdown()
	{
		ImGui_ImplGlfw_Shutdown();

		// Wait for GPU to finish
		if (m_Fence && m_Queue)
		{
			m_FenceValue++;
			m_Queue->Signal(m_Fence, m_FenceValue);
			if (m_Fence->GetCompletedValue() < m_FenceValue)
			{
				m_Fence->SetEventOnCompletion(m_FenceValue, m_FenceEvent);
				WaitForSingleObject(m_FenceEvent, INFINITE);
			}
		}

		for (int i = 0; i < 2; ++i)
		{
			if (m_FrameCmdLists[i])   m_FrameCmdLists[i]->Release();
			if (m_FrameAllocators[i]) m_FrameAllocators[i]->Release();
		}
		if (m_GameUICmdList)   m_GameUICmdList->Release();
		if (m_GameUIAllocator) m_GameUIAllocator->Release();
		// m_SRVHeap points at the device's shared heap (owned by D3D12Device),
		// so it must NOT be released here — null it instead.
		m_SRVHeap = nullptr;
		if (m_Fence) m_Fence->Release();

		if (m_FenceEvent)
		{
			CloseHandle(m_FenceEvent);
			m_FenceEvent = nullptr;
		}

		m_Device = nullptr;
		m_Queue  = nullptr;
		m_Fence  = nullptr;
		m_FenceValue = 0;
		m_FrameAllocators[0] = m_FrameAllocators[1] = nullptr;
		m_FrameCmdLists[0]   = m_FrameCmdLists[1]   = nullptr;
		m_GameUIAllocator = nullptr;
		m_GameUICmdList   = nullptr;
		m_EditorContext = m_GameUIContext = nullptr;
		m_EditorSRVBase = m_GameUISRVBase = 0;
		m_SRVHeapUsedEditor = m_SRVHeapUsedGameUI = 0;
		m_SRVDescSize = 0;
	}

	// =========================================================================
	// Frame lifecycle
	// =========================================================================

	void D3D12ImGuiBackend::NewFrame()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplGlfw_NewFrame();
	}

	void D3D12ImGuiBackend::NewFrameGameUI()
	{
		// Renderer only — no GLFW platform frame for the overlay context.
		ImGui_ImplDX12_NewFrame();
	}

	void D3D12ImGuiBackend::RenderDrawData(ImDrawData* drawData, Framebuffer* target)
	{
		if (target)
		{
			// The game UI belongs INSIDE the viewport: composite it into the
			// target framebuffer instead of presenting the swap chain.
			RenderToFramebuffer(drawData, dynamic_cast<D3D12Framebuffer*>(target));
			return;
		}
		
		// Multi-viewport platform windows (drag ImGui windows out of the main window).
		// The imgui_impl_dx12 backend self-manages per-viewport swap chains using the
		// Device/CommandQueue from InitInfo; we only need to drive the per-frame update.
		RenderToSwapChain(drawData);
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

	void D3D12ImGuiBackend::RenderToSwapChain(ImDrawData* drawData)
	{
		if (!drawData || drawData->CmdListsCount == 0)
			return;

		auto* gfxCtx = static_cast<D3D12GraphicsContext*>(
			static_cast<WindowsWindow*>(&Application::Get().GetWindow())
				->GetGraphicsContext());
		auto* sc = gfxCtx ? gfxCtx->GetSwapChain() : nullptr;
		if (!sc)
		{
			CANDY_CORE_ERROR("D3D12ImGuiBackend::RenderToSwapChain — no D3D12 swap chain bound");
			return;
		}

		// The window may have been resized after the last event poll (e.g. the
		// OS resized it mid-frame). Flip-model swapchains require the back buffer
		// to match the window size — presenting a stale-size buffer hangs the GPU
		// (DXGI_ERROR_DEVICE_HUNG). Resize the swap chain now, before we capture
		// the back buffer, so this frame always presents a matching size.
		gfxCtx->EnsureSwapChainMatchesWindow();

		// Use the swap chain's CURRENT back-buffer index as the frame index, exactly
		// like the imgui example_dx12. DXGI guarantees the current back buffer is
		// available to render into; using our own independent counter can desync and
		// make us render into a buffer that is still being displayed (present queued)
		// → the GPU hangs (DXGI_ERROR_DEVICE_HUNG) when that buffer is presented.
		uint32_t fi = sc->GetSwapChain()->GetCurrentBackBufferIndex();

		// Wait for previous frame to complete
		if (m_Fence->GetCompletedValue() < m_FenceValue)
		{
			m_Fence->SetEventOnCompletion(m_FenceValue, m_FenceEvent);
			WaitForSingleObject(m_FenceEvent, INFINITE);
		}

		// Reset allocator and command list
		m_FrameAllocators[fi]->Reset();
		m_FrameCmdLists[fi]->Reset(m_FrameAllocators[fi], nullptr);

		// Set descriptor heaps — bind the device's shared CBV_SRV_UAV heap. All
		// SRVs ImGui displays (fonts, viewport framebuffer, engine textures)
		// live in this heap, so their handles are only valid while it is bound.
		ID3D12DescriptorHeap* heaps[] = { m_SRVHeap };
		m_FrameCmdLists[fi]->SetDescriptorHeaps(1, heaps);

		// ---------------------------------------------------------------
		// Bind the swap-chain back buffer as the ImGui render target:
		// PRESENT → RENDER_TARGET barrier, OMSetRenderTargets to its RTV,
		// then clear to the engine's editor clear color.
		// ---------------------------------------------------------------
		ID3D12Resource*           backBuffer = sc->GetCurrentBackBufferResource();
		D3D12_CPU_DESCRIPTOR_HANDLE rtv       = sc->GetCurrentRTVHandle();

		// Defensive: a removed device or a broken swap chain (e.g. a failed
		// ResizeBuffers left it without back buffers) would make the barrier /
		// ClearRenderTargetView below crash. Detect it and skip the frame.
		if (!backBuffer || !sc->GetRTVHeap())
		{
			CANDY_CORE_ERROR("D3D12ImGuiBackend::RenderToSwapChain — swap chain has no back buffer/RTV; skipping frame");
			return;
		}
		if (gfxCtx->GetDevice() && gfxCtx->GetDevice()->GetNativeDevice())
		{
			HRESULT removed = gfxCtx->GetDevice()->GetNativeDevice()->GetDeviceRemovedReason();
			if (removed != S_OK)
			{
				CANDY_CORE_ERROR("D3D12ImGuiBackend::RenderToSwapChain — device removed (0x{:08X}); skipping frame",
				                 static_cast<uint32_t>(removed));
				gfxCtx->GetDevice()->LogDebugLayerMessages();
				return;
			}
		}

		D3D12_RESOURCE_BARRIER inBarrier = {};
		inBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		inBarrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		inBarrier.Transition.pResource   = backBuffer;
		inBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		inBarrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
		inBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		m_FrameCmdLists[fi]->ResourceBarrier(1, &inBarrier);

		static const float kClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
		m_FrameCmdLists[fi]->ClearRenderTargetView(rtv, kClearColor, 0, nullptr);
		m_FrameCmdLists[fi]->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		// Render ImGui draw data
		ImGui_ImplDX12_RenderDrawData(drawData, m_FrameCmdLists[fi]);

		// RENDER_TARGET → PRESENT barrier before Present.
		D3D12_RESOURCE_BARRIER outBarrier = inBarrier;
		outBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		outBarrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
		m_FrameCmdLists[fi]->ResourceBarrier(1, &outBarrier);

		m_FrameCmdLists[fi]->Close();

		// Submit
		ID3D12CommandList* lists[] = { m_FrameCmdLists[fi] };
		m_Queue->ExecuteCommandLists(1, lists);

		// Signal fence for this frame
		m_FenceValue++;
		m_Queue->Signal(m_Fence, m_FenceValue);

		// Present the D3D12 swap chain
		sc->GetSwapChain()->Present(sc->GetDesc().VSync ? 1u : 0u, 0u);
		sc->AdvanceFrame();

		// Signal the resize fence after Present so a window resize can safely
		// drain this frame before ResizeBuffers (avoids DXGI_ERROR_DEVICE_REMOVED).
		sc->SignalAfterPresent();
	}

	void D3D12ImGuiBackend::RenderToFramebuffer(ImDrawData* drawData, D3D12Framebuffer* fb)
	{
		if (!drawData || drawData->CmdListsCount == 0)
			return;
		if (!fb)
		{
			RenderToSwapChain(drawData);
			return;
		}

		// The game UI uses its own dedicated allocator/list (it renders into the
		// viewport framebuffer, not the swap chain, so it must not share the
		// editor's swap-chain-bound frame state).
		ID3D12CommandAllocator*    guiAlloc = m_GameUIAllocator;
		ID3D12GraphicsCommandList* guiCmd   = m_GameUICmdList;
		if (!guiAlloc || !guiCmd)
			return;

		// Wait for the previous game UI submission.
		if (m_Fence->GetCompletedValue() < m_FenceValue)
		{
			m_Fence->SetEventOnCompletion(m_FenceValue, m_FenceEvent);
			WaitForSingleObject(m_FenceEvent, INFINITE);
		}

		// Reset allocator and command list
		guiAlloc->Reset();
		guiCmd->Reset(guiAlloc, nullptr);

		// Bind the device's shared CBV_SRV_UAV heap (same as RenderToSwapChain).
		ID3D12DescriptorHeap* heaps[] = { m_SRVHeap };
		guiCmd->SetDescriptorHeaps(1, heaps);

		// Render into the framebuffer's color attachment 0. Renderer2D's EndRenderPass
		// leaves the attachment in PIXEL_SHADER_RESOURCE (so the editor can display it),
		// so we must explicitly transition it back to RENDER_TARGET first — otherwise
		// D3D12 renders into a texture in the wrong state and the GPU faults/hangs.
		fb->EnsureColorAttachmentState(guiCmd, 0, D3D12_RESOURCE_STATE_RENDER_TARGET);

		D3D12_CPU_DESCRIPTOR_HANDLE rtv = fb->GetRTVHandle(0);
		guiCmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		ImGui_ImplDX12_RenderDrawData(drawData, guiCmd);

		// Back to PIXEL_SHADER_RESOURCE so the editor ImGui pass can read the viewport
		// image from this framebuffer.
		fb->EnsureColorAttachmentState(guiCmd, 0, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		guiCmd->Close();

		// Execute + signal fence. NO Present — the swap chain is presented exactly
		// once per frame by the editor's RenderToSwapChain.
		ID3D12CommandList* lists[] = { guiCmd };
		m_Queue->ExecuteCommandLists(1, lists);

		m_FenceValue++;
		m_Queue->Signal(m_Fence, m_FenceValue);
	}
}
