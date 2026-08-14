#pragma once

#include "Runtime/Imgui/ImGuiBackend.h"

#include <d3d12.h>

struct ImGuiContext;
struct ImGui_ImplDX12_InitInfo;

namespace Candy {

	class D3D12Framebuffer;

	// =========================================================================
	// D3D12ImGuiBackend — ImGui_ImplDX12 adapter
	//
	// Owns everything the editor UI and game UI need to render via D3D12:
	// per-frame command allocators/lists (mirroring swap-chain back buffers),
	// a fence for CPU/GPU overlap, a dedicated game-UI command list (renders
	// into the viewport framebuffer, never presents), and the SRV descriptor
	// allocator shared by both ImGui contexts.
	// =========================================================================
	class D3D12ImGuiBackend : public ImGuiBackend
	{
	public:
		void Init(GLFWwindow* window) override;
		void InitContext() override;
		void ShutdownContext() override;
		void Shutdown() override;

		bool SupportsPlatformWindows() const override { return false; }

		void NewFrame() override;
		void NewFrameGameUI() override;
		void RenderDrawData(ImDrawData* drawData, Framebuffer* target) override;

	private:
		void RenderToSwapChain(ImDrawData* drawData);
		void RenderToFramebuffer(ImDrawData* drawData, D3D12Framebuffer* fb);

		static void SRVAllocator(ImGui_ImplDX12_InitInfo* info,
		                         D3D12_CPU_DESCRIPTOR_HANDLE* outCPU,
		                         D3D12_GPU_DESCRIPTOR_HANDLE* outGPU);
		static void SRVDeallocator(ImGui_ImplDX12_InitInfo* info,
		                           D3D12_CPU_DESCRIPTOR_HANDLE cpu,
		                           D3D12_GPU_DESCRIPTOR_HANDLE gpu);

		ID3D12Device*              m_Device      = nullptr;
		ID3D12CommandQueue*        m_Queue       = nullptr;
		// Points at the device's shared CBV_SRV_UAV heap (owned by D3D12Device).
		ID3D12DescriptorHeap*      m_SRVHeap     = nullptr;
		uint32_t                   m_SRVDescSize = 0;
		// Per-context descriptor region bases, handed out by the device's IR
		// descriptor range allocator at Init (32 slots each), plus the bump
		// counters within those regions.
		uint32_t                   m_EditorSRVBase     = 0;
		uint32_t                   m_GameUISRVBase     = 0;
		uint32_t                   m_SRVHeapUsedEditor = 0;
		uint32_t                   m_SRVHeapUsedGameUI = 0;

		// Per-frame resources (mirror swap-chain back buffers for GPU-CPU overlap)
		ID3D12CommandAllocator*    m_FrameAllocators[2] = { nullptr, nullptr };
		ID3D12GraphicsCommandList* m_FrameCmdLists[2]   = { nullptr, nullptr };
		ID3D12Fence*               m_Fence        = nullptr;
		uint64_t                   m_FenceValue   = 0;
		HANDLE                     m_FenceEvent   = nullptr;

		// Dedicated game UI allocator/list. The game UI renders into the viewport
		// framebuffer (no Present) and must NOT share the editor's swapchain-bound
		// frame state (which uses the swap chain's back-buffer index).
		ID3D12CommandAllocator*    m_GameUIAllocator = nullptr;
		ID3D12GraphicsCommandList* m_GameUICmdList   = nullptr;

		// Context bookkeeping: first InitContext binds the editor, second the game UI.
		ImGuiContext* m_EditorContext = nullptr;
		ImGuiContext* m_GameUIContext = nullptr;
	};
}
