#pragma once

#include "Runtime/Core/Layer.h"
#include "Runtime/Events/ApplicationEvent.h"
#include "Runtime/Events/KeyEvent.h"
#include "Runtime/Events/MouseEvent.h"

struct ImGuiContext;
struct ImDrawData;
struct ImGuiIO;

// DX12 forward declarations (Windows-only)
#ifdef CANDY_PLATFORM_WINDOWS
struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12DescriptorHeap;
struct ID3D12CommandAllocator;
struct ID3D12GraphicsCommandList;
struct ID3D12Fence;
#endif

namespace Candy {

	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnEvent(Event& e) override;

		void Begin();
		void End();
		void BlockEvents(bool block) { m_BlockEvents = block; }

		void SetDarkThemeColors();
		static void RebuildFont(const std::string& fontPath);

		// Reload fonts from VFS for both editor and game UI contexts.
		void ReloadFontsFromVfs();

		// Disable the editor context's chrome in standalone game mode.
		void DisableEditorChrome();
		bool m_EditorChromeDisabled = false;

		// Game UI context
		ImGuiContext* GetGameUIContext() const { return m_GameUIContext; }
		ImGuiContext* GetEditorContext() const { return m_EditorContext; }
		void BeginGameUI(float displayW, float displayH, float mouseX, float mouseY, bool mouseDown, float deltaTime);
		void EndGameUI();
		ImDrawData* GetGameUIDrawData();
		bool GameUIWantsMouse() const;
	private:
		bool m_BlockEvents = true;
		float m_Time = 0.0f;

		ImGuiContext* m_EditorContext = nullptr;
		ImGuiContext* m_GameUIContext = nullptr;

		void LoadFontsFromVfs(ImGuiIO& targetIO);

		// Backend detection
		bool IsDX12Backend() const;

#ifdef CANDY_PLATFORM_WINDOWS
		// ---- DX12 ImGui backend state -------------------------------------

		struct DX12ImGuiState
		{
			ID3D12Device*              Device      = nullptr;
			ID3D12CommandQueue*        Queue       = nullptr;
			ID3D12DescriptorHeap*      SRVHeap     = nullptr;
			uint32_t                   SRVDescSize = 0;
			uint32_t                   SRVHeapUsed = 0;

			// Per-frame resources (double-buffered for GPU-CPU overlap)
			ID3D12CommandAllocator*    FrameAllocators[2] = { nullptr, nullptr };
			ID3D12GraphicsCommandList* FrameCmdLists[2]   = { nullptr, nullptr };
			ID3D12Fence*               Fence        = nullptr;
			uint64_t                   FenceValue   = 0;
			HANDLE                     FenceEvent   = nullptr;
			uint32_t                   FrameIndex   = 0;

			// Store previous context for context switching
			ImGuiContext* GameUIContext = nullptr;
		};
		DX12ImGuiState m_DX12;
		bool m_IsDX12 = false;

		void InitDX12Backend(GLFWwindow* window);
		void ShutdownDX12Backend();
		void NewFrameDX12();
		void RenderDX12(ImDrawData* drawData);
		void CreateDX12FontTexture();

		// SRV descriptor allocator for ImGui
		static void SRVAllocator(ImGui_ImplDX12_InitInfo* info,
		                         D3D12_CPU_DESCRIPTOR_HANDLE* outCPU,
		                         D3D12_GPU_DESCRIPTOR_HANDLE* outGPU);
#endif
	};

}
