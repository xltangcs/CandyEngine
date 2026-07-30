#include "CandyPCH.h"

#include "Runtime/Core/Application.h"
#include "Runtime/Core/FileSystem.h"
#include "Runtime/Renderer/Renderer.h"

#include "Runtime/Imgui/ImguiLayer.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include "ImGuizmo.h"
#include <GLFW/glfw3.h>
#include <filesystem>

#include "imgui_internal.h"

#ifdef CANDY_PLATFORM_WINDOWS
#include <backends/imgui_impl_dx12.h>
#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#include <backends/imgui_impl_vulkan.h>
#include "Platform/D3D12/D3D12GraphicsContext.h"
#include "Platform/D3D12/D3D12Device.h"
#include "Platform/D3D12/D3D12SwapChain.h"
#include "Platform/Vulkan/VulkanGraphicsContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanSwapChain.h"
#include "Platform/Windows/WindowsWindow.h"
#endif

#include <glad/glad.h>
#include <backends/imgui_impl_opengl3.h>

namespace Candy {

	// =========================================================================
	// Construction / destruction
	// =========================================================================

	ImGuiLayer::ImGuiLayer()
	{
	}

	// =========================================================================
	// Backend detection
	// =========================================================================

	bool ImGuiLayer::IsD3D12Backend() const
	{
		return m_IsD3D12;
	}

	// =========================================================================
	// OnAttach
	// =========================================================================

	void ImGuiLayer::OnAttach()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		m_EditorContext = ImGui::GetCurrentContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		std::filesystem::create_directories("Saved");
		io.IniFilename = "Saved/imgui.ini";

		LoadFontsFromVfs(io);

		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		SetDarkThemeColors();

		Application& app = Application::Get();
		GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());

		// Detect backend
		m_IsD3D12   = (Renderer::GetAPI() == RendererAPI::API::D3D12);
		m_IsVulkan = (Renderer::GetAPI() == RendererAPI::API::Vulkan);

		// Multi-viewport platform rendering is only wired through the
		// OpenGL/Glfw backend (UpdatePlatformWindowsDefault path).  D3D12 and
		// Vulkan backends do not yet hook RenderPlatformWindowsDefault, so
		// leaving the flag enabled would trip the
		// `Forgot to call UpdatePlatformWindows()` assertion in imgui.cpp.
		if (m_IsD3D12 || m_IsVulkan)
			io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

		if (m_IsVulkan)
		{
			ImGui_ImplGlfw_InitForVulkan(window, true);
			InitVulkanBackend(window);
			CANDY_CORE_INFO("ImGuiLayer: Vulkan backend initialized");
		}
		else if (m_IsD3D12)
		{
#ifdef CANDY_PLATFORM_WINDOWS
			// GLFW platform backend (no OpenGL context)
			ImGui_ImplGlfw_InitForOther(window, true);

			// D3D12 renderer backend
			InitD3D12Backend(window);

			CANDY_CORE_INFO("ImGuiLayer: D3D12 backend initialized");
#endif
		}
		else
		{
			ImGui_ImplGlfw_InitForOpenGL(window, true);
			ImGui_ImplOpenGL3_Init("#version 410");
		}

		// Create game UI context
		m_GameUIContext = ImGui::CreateContext();
		ImGui::SetCurrentContext(m_GameUIContext);

		if (m_IsD3D12)
		{
#ifdef CANDY_PLATFORM_WINDOWS
			m_D3D12.GameUIContext = m_GameUIContext;
			// Per-context ImGui_DX12 backend init for the game UI context.
			// InitD3D12Backend is idempotent + current-context guarded so this
			// only calls ImGui_ImplDX12_Init once for this context; shared
			// resources stay on m_D3D12.
			InitD3D12Backend(window);
			CANDY_CORE_INFO("ImGuiLayer: D3D12 backend initialized (game UI context)");
#endif
		}
		else
		{
			ImGui_ImplOpenGL3_Init("#version 410");
		}

		ImGuiIO& gameIO = ImGui::GetIO();
		gameIO.IniFilename = nullptr;
		gameIO.ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;
		gameIO.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
		gameIO.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
		LoadFontsFromVfs(gameIO);
		ImGui::SetCurrentContext(m_EditorContext);
	}

	// =========================================================================
	// OnDetach
	// =========================================================================

	void ImGuiLayer::OnDetach()
	{
		if (m_IsVulkan)
		{
#ifdef CANDY_PLATFORM_WINDOWS
			ImGui::SetCurrentContext(m_GameUIContext);
			ImGui_ImplVulkan_Shutdown();
			ImGui::SetCurrentContext(m_EditorContext);
			ImGui_ImplVulkan_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ShutdownVulkanBackend();
#endif
		}
		else if (m_IsD3D12)
		{
#ifdef CANDY_PLATFORM_WINDOWS
			ImGui::SetCurrentContext(m_GameUIContext);
			ImGui_ImplDX12_Shutdown();
			ImGui::SetCurrentContext(m_EditorContext);
			ImGui_ImplDX12_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ShutdownD3D12Backend();
#endif
		}
		else
		{
			ImGui::SetCurrentContext(m_GameUIContext);
			ImGui_ImplOpenGL3_Shutdown();
			ImGui::SetCurrentContext(m_EditorContext);
			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplGlfw_Shutdown();
		}

		ImGui::DestroyContext(m_GameUIContext);
		ImGui::DestroyContext(m_EditorContext);
	}

	// =========================================================================
	// OnEvent
	// =========================================================================

	void ImGuiLayer::OnEvent(Event& e)
	{
		if (m_BlockEvents)
		{
			ImGuiIO& io = ImGui::GetIO();
			e.Handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
			e.Handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
		}
	}

	// =========================================================================
	// Begin / End
	// =========================================================================

	void ImGuiLayer::Begin()
	{
		if (m_EditorChromeDisabled)
			return;

		if (m_IsVulkan)
		{
			NewFrameVulkan();
		}
		else if (m_IsD3D12)
		{
#ifdef CANDY_PLATFORM_WINDOWS
			NewFrameD3D12();
#endif
		}
		else
		{
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
		}

		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void ImGuiLayer::End()
	{
		if (m_EditorChromeDisabled)
			return;

		ImGuiIO& io = ImGui::GetIO();
		Application& app = Application::Get();
		io.DisplaySize = ImVec2((float)app.GetWindow().GetWidth(), (float)app.GetWindow().GetHeight());

		ImGui::Render();

		if (m_IsVulkan)
		{
			RenderVulkan(ImGui::GetDrawData());
		}
		else if (m_IsD3D12)
		{
#ifdef CANDY_PLATFORM_WINDOWS
			RenderD3D12(ImGui::GetDrawData());
#endif
		}
		else
		{
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		if (!m_IsD3D12 && (Renderer::GetAPI() != RendererAPI::API::Vulkan)
		    && (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
		{
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
	}

	// =========================================================================
	// Game UI
	// =========================================================================

	void ImGuiLayer::BeginGameUI(float displayW, float displayH, float mouseX, float mouseY, bool mouseDown, float deltaTime)
	{
		ImGui::SetCurrentContext(m_GameUIContext);
		ImGuiIO& io = ImGui::GetIO();

		io.DisplaySize = ImVec2(displayW, displayH);
		io.DeltaTime = deltaTime;
		io.MousePos = ImVec2(mouseX, mouseY);
		io.MouseDown[0] = mouseDown;

		if (m_IsD3D12)
		{
#ifdef CANDY_PLATFORM_WINDOWS
			ImGui_ImplDX12_NewFrame();
#endif
		}
		else
		{
#ifndef CANDY_PLATFORM_WINDOWS
			ImGui_ImplOpenGL3_NewFrame();
#endif
		}

		ImGui::NewFrame();
	}

	void ImGuiLayer::EndGameUI()
	{
		ImGui::SetCurrentContext(m_GameUIContext);
		ImGui::Render();

		if (m_IsD3D12)
		{
#ifdef CANDY_PLATFORM_WINDOWS
			RenderD3D12(ImGui::GetDrawData());
#endif
		}
		else
		{
#ifndef CANDY_PLATFORM_WINDOWS
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
		}

		ImGui::SetCurrentContext(m_EditorContext);
	}

	ImDrawData* ImGuiLayer::GetGameUIDrawData()
	{
		ImGui::SetCurrentContext(m_GameUIContext);
		ImDrawData* dd = ImGui::GetDrawData();
		ImGui::SetCurrentContext(m_EditorContext);
		return dd;
	}

	bool ImGuiLayer::GameUIWantsMouse() const
	{
		if (!m_GameUIContext) return false;
		ImGuiContext* prev = ImGui::GetCurrentContext();
		ImGui::SetCurrentContext(m_GameUIContext);
		bool wants = ImGui::GetIO().WantCaptureMouse;
		ImGui::SetCurrentContext(prev);
		return wants;
	}

	// =========================================================================
	// Font loading
	// =========================================================================

	void ImGuiLayer::LoadFontsFromVfs(ImGuiIO& targetIO)
	{
		auto boldData = FileSystem::Get().Read("VFS://Engine/Fonts/opensans/OpenSans-Bold.ttf");
		if (boldData && !boldData->empty())
		{
			void* fontMem = ImGui::MemAlloc(boldData->size());
			memcpy(fontMem, boldData->data(), boldData->size());
			targetIO.Fonts->AddFontFromMemoryTTF(fontMem, (int)boldData->size(), 18.0f);
		}

		auto regularData = FileSystem::Get().Read("VFS://Engine/Fonts/opensans/OpenSans-Regular.ttf");
		if (regularData && !regularData->empty())
		{
			void* fontMem = ImGui::MemAlloc(regularData->size());
			memcpy(fontMem, regularData->data(), regularData->size());
			targetIO.FontDefault = targetIO.Fonts->AddFontFromMemoryTTF(fontMem, (int)regularData->size(), 18.0f);
		}
		else
			targetIO.FontDefault = targetIO.Fonts->AddFontDefault();
	}

	void ImGuiLayer::ReloadFontsFromVfs()
	{
		ImGui::SetCurrentContext(m_EditorContext);
		ImGuiIO& editorIO = ImGui::GetIO();
		editorIO.Fonts->Clear();
		LoadFontsFromVfs(editorIO);

		if (m_GameUIContext)
		{
			ImGui::SetCurrentContext(m_GameUIContext);
			ImGuiIO& gameIO = ImGui::GetIO();
			gameIO.Fonts->Clear();
			LoadFontsFromVfs(gameIO);
		}

		ImGui::SetCurrentContext(m_EditorContext);
	}

	void ImGuiLayer::DisableEditorChrome()
	{
		m_EditorChromeDisabled = true;
		ImGui::SetCurrentContext(m_EditorContext);
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = nullptr;
		ImGui::ClearIniSettings();
		ImGui::SetCurrentContext(m_EditorContext);
	}

	void ImGuiLayer::RebuildFont(const std::string& fontPath)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->ClearFonts();

		if (!fontPath.empty() && fontPath.starts_with("VFS://"))
		{
			auto data = FileSystem::Get().Read(fontPath);
			if (data && !data->empty())
			{
				void* fontMem = ImGui::MemAlloc(data->size());
				memcpy(fontMem, data->data(), data->size());
				io.FontDefault = io.Fonts->AddFontFromMemoryTTF(fontMem, (int)data->size(), 18.0f);
			}
			else
				io.FontDefault = io.Fonts->AddFontDefault();
		}
		else if (std::filesystem::exists(fontPath))
			io.FontDefault = io.Fonts->AddFontFromFileTTF(fontPath.c_str());
		else
			io.FontDefault = io.Fonts->AddFontDefault();
	}

	void ImGuiLayer::SetDarkThemeColors()
	{
		auto& colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_WindowBg] = ImVec4{ 0.1f, 0.105f, 0.11f, 1.0f };
		colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
		colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
		colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	}

	// =========================================================================
	// D3D12 Backend Implementation (Windows only)
	// =========================================================================

#ifdef CANDY_PLATFORM_WINDOWS

	void ImGuiLayer::SRVAllocator(ImGui_ImplDX12_InitInfo* info,
	                              D3D12_CPU_DESCRIPTOR_HANDLE* outCPU,
	                              D3D12_GPU_DESCRIPTOR_HANDLE* outGPU)
	{
		auto* self = static_cast<ImGuiLayer*>(info->UserData);
		auto& s = self->m_D3D12;

		// Linear allocation from the SRV heap (simple, resets each frame)
		D3D12_CPU_DESCRIPTOR_HANDLE cpu = s.SRVHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE gpu = s.SRVHeap->GetGPUDescriptorHandleForHeapStart();

		cpu.ptr += static_cast<SIZE_T>(s.SRVHeapUsed) * s.SRVDescSize;
		gpu.ptr += static_cast<SIZE_T>(s.SRVHeapUsed) * s.SRVDescSize;

		s.SRVHeapUsed++;

		*outCPU = cpu;
		*outGPU = gpu;
	}

	void ImGuiLayer::SRVDeallocator(ImGui_ImplDX12_InitInfo* /*info*/,
	                                D3D12_CPU_DESCRIPTOR_HANDLE /*cpu*/,
	                                D3D12_GPU_DESCRIPTOR_HANDLE /*gpu*/)
	{
		// ImGui_ImplDX12_InitInfo requires both Alloc and Free callbacks to be
		// non-null (see assertion in ImGui_ImplDX12_Init).  Our allocator is a
		// simple linear bump that resets every frame, so nothing to release.
	}

	void ImGuiLayer::InitD3D12Backend(GLFWwindow* window)
	{
		// ---------------------------------------------------------------------------
		// Idempotent per-context init:
		//   - ImGui_ImplDX12 backend data lives in io.BackendRendererUserData of
		//     the *current* ImGui context. ImGui_ImplDX12_Init asserts
		//     `io.BackendRendererUserData == nullptr && "Already initialized a
		//     renderer backend!"` so we early-return when this context already
		//     has a backend.
		//   - Shared physical resources (device/queue, SRV heap, per-frame
		//     command allocators/lists, fence) are created exactly once because
		//     they live on the single m_D3D12 struct that backs both the editor
		//     context and the game UI context.
		// ---------------------------------------------------------------------------

		ImGuiIO& io = ImGui::GetIO();
		if (io.BackendRendererUserData != nullptr)
			return; // backend already initialized for this context

		auto* win    = static_cast<WindowsWindow*>(&Application::Get().GetWindow());
		auto* gfxCtx = static_cast<D3D12GraphicsContext*>(win->GetGraphicsContext());

		m_D3D12.Device = gfxCtx->GetDevice()->GetNativeDevice();
		m_D3D12.Queue  = gfxCtx->GetDevice()->GetNativeQueue();

		// SRV descriptor heap for ImGui textures (shared by both ImGui contexts).
		if (!m_D3D12.SRVHeap)
		{
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
			heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.NumDescriptors = 16;
			heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			HRESULT hr = m_D3D12.Device->CreateDescriptorHeap(
				&heapDesc, IID_PPV_ARGS(&m_D3D12.SRVHeap));
			if (FAILED(hr))
			{
				CANDY_CORE_ERROR("ImGuiLayer::InitD3D12Backend: SRV heap creation failed");
				return;
			}

			m_D3D12.SRVDescSize = m_D3D12.Device->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}

		// Per-frame command lists (created once, shared across both contexts —
		// they iterate sequentially per main loop so they do not stomp each
		// other).
		if (!m_D3D12.FrameAllocators[0])
		{
			for (int i = 0; i < 2; ++i)
			{
				HRESULT hr = m_D3D12.Device->CreateCommandAllocator(
					D3D12_COMMAND_LIST_TYPE_DIRECT,
					IID_PPV_ARGS(&m_D3D12.FrameAllocators[i]));
				if (FAILED(hr))
				{
					CANDY_CORE_ERROR("ImGuiLayer: CreateCommandAllocator[{0}] failed", i);
					return;
				}

				hr = m_D3D12.Device->CreateCommandList(
					0, D3D12_COMMAND_LIST_TYPE_DIRECT,
					m_D3D12.FrameAllocators[i], nullptr,
					IID_PPV_ARGS(&m_D3D12.FrameCmdLists[i]));
				if (FAILED(hr))
				{
					CANDY_CORE_ERROR("ImGuiLayer: CreateCommandList[{0}] failed", i);
					return;
				}

				m_D3D12.FrameCmdLists[i]->Close();
			}

			m_D3D12.Device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
			                            IID_PPV_ARGS(&m_D3D12.Fence));
			m_D3D12.FenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
		}

		// Initialize ImGui D3D12 backend for the *current* context. Safe to run
		// per-context since the Io guard at the top ensures it only runs when
		// the context doesn't yet have a backend bound.
		ImGui_ImplDX12_InitInfo initInfo = {};
		initInfo.Device               = m_D3D12.Device;
		initInfo.CommandQueue          = m_D3D12.Queue;
		initInfo.NumFramesInFlight     = 2;
		initInfo.RTVFormat            = DXGI_FORMAT_B8G8R8A8_UNORM;
		initInfo.DSVFormat            = DXGI_FORMAT_UNKNOWN;
		initInfo.SrvDescriptorHeap    = m_D3D12.SRVHeap;
		initInfo.SrvDescriptorAllocFn = SRVAllocator;
		initInfo.SrvDescriptorFreeFn  = SRVDeallocator;
		initInfo.UserData             = this;

		if (!ImGui_ImplDX12_Init(&initInfo))
		{
			CANDY_CORE_ERROR("ImGuiLayer: ImGui_ImplDX12_Init failed");
		}
	}

	void ImGuiLayer::ShutdownD3D12Backend()
	{
		// Wait for GPU to finish
		if (m_D3D12.Fence && m_D3D12.Queue)
		{
			m_D3D12.FenceValue++;
			m_D3D12.Queue->Signal(m_D3D12.Fence, m_D3D12.FenceValue);
			if (m_D3D12.Fence->GetCompletedValue() < m_D3D12.FenceValue)
			{
				m_D3D12.Fence->SetEventOnCompletion(m_D3D12.FenceValue, m_D3D12.FenceEvent);
				WaitForSingleObject(m_D3D12.FenceEvent, INFINITE);
			}
		}

		for (int i = 0; i < 2; ++i)
		{
			if (m_D3D12.FrameCmdLists[i])   m_D3D12.FrameCmdLists[i]->Release();
			if (m_D3D12.FrameAllocators[i]) m_D3D12.FrameAllocators[i]->Release();
		}
		if (m_D3D12.SRVHeap) m_D3D12.SRVHeap->Release();
		if (m_D3D12.Fence)   m_D3D12.Fence->Release();

		if (m_D3D12.FenceEvent)
		{
			CloseHandle(m_D3D12.FenceEvent);
			m_D3D12.FenceEvent = nullptr;
		}

		memset(&m_D3D12, 0, sizeof(m_D3D12));
	}

	void ImGuiLayer::NewFrameD3D12()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplGlfw_NewFrame();
	}

	void ImGuiLayer::RenderD3D12(ImDrawData* drawData)
	{
		if (!drawData || drawData->CmdListsCount == 0)
			return;

		uint32_t fi = m_D3D12.FrameIndex;

		// Wait for previous frame to complete
		if (m_D3D12.Fence->GetCompletedValue() < m_D3D12.FenceValue)
		{
			m_D3D12.Fence->SetEventOnCompletion(m_D3D12.FenceValue, m_D3D12.FenceEvent);
			WaitForSingleObject(m_D3D12.FenceEvent, INFINITE);
		}

		// Reset allocator and command list
		m_D3D12.FrameAllocators[fi]->Reset();
		m_D3D12.FrameCmdLists[fi]->Reset(m_D3D12.FrameAllocators[fi], nullptr);

		// Set descriptor heaps
		ID3D12DescriptorHeap* heaps[] = { m_D3D12.SRVHeap };
		m_D3D12.FrameCmdLists[fi]->SetDescriptorHeaps(1, heaps);

		// Render ImGui draw data
		ImGui_ImplDX12_RenderDrawData(drawData, m_D3D12.FrameCmdLists[fi]);

		m_D3D12.FrameCmdLists[fi]->Close();

		// Submit
		ID3D12CommandList* lists[] = { m_D3D12.FrameCmdLists[fi] };
		m_D3D12.Queue->ExecuteCommandLists(1, lists);

		// Signal fence for this frame
		m_D3D12.FenceValue++;
		m_D3D12.Queue->Signal(m_D3D12.Fence, m_D3D12.FenceValue);

		// Present the D3D12 swap chain
		auto* gfxCtx = static_cast<D3D12GraphicsContext*>(
			static_cast<WindowsWindow*>(&Application::Get().GetWindow())
				->GetGraphicsContext());

		if (auto* sc = gfxCtx->GetSwapChain())
		{
			UINT flags = 0;
			sc->GetSwapChain()->Present(1, flags);
			sc->AdvanceFrame();
		}

		// Advance frame index (double-buffered)
		m_D3D12.FrameIndex = (fi + 1) % 2;

		// Reset SRV descriptor allocator
		m_D3D12.SRVHeapUsed = 0;
	}

	// =========================================================================
	// Vulkan ImGui backend
	// =========================================================================

	void ImGuiLayer::InitVulkanBackend(GLFWwindow* window)
	{
		auto* gfxCtx = static_cast<VulkanGraphicsContext*>(
			static_cast<WindowsWindow*>(&Application::Get().GetWindow())->GetGraphicsContext());
		auto* vkDev  = gfxCtx->GetDevice();
		auto* vkSC   = gfxCtx->GetSwapChain();

		VkDevice device = vkDev->GetVkDevice();

		// Create descriptor pool for ImGui
		{
			VkDescriptorPoolSize poolSizes[] = {
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16 },
			};
			VkDescriptorPoolCreateInfo dpci = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
			dpci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			dpci.maxSets       = 16;
			dpci.poolSizeCount = 1;
			dpci.pPoolSizes    = poolSizes;

			VkDescriptorPool pool;
			vkDev->fnCreateDescriptorPool(device, &dpci, nullptr, &pool);
			m_VkDescriptorPool = pool;
		}

		// Init info
		ImGui_ImplVulkan_InitInfo initInfo = {};
		initInfo.Instance        = vkDev->GetVkInstance();
		initInfo.PhysicalDevice  = vkDev->GetVkPhysicalDevice();
		initInfo.Device          = device;
		initInfo.QueueFamily     = vkDev->GetGraphicsQueueFamilyIndex();
		initInfo.Queue           = vkDev->GetVkQueue();
		initInfo.DescriptorPool  = static_cast<VkDescriptorPool>(m_VkDescriptorPool);
		initInfo.MinImageCount   = 2;
		initInfo.ImageCount      = 2;
		initInfo.PipelineInfoMain.RenderPass = vkSC->GetRenderPass();
		initInfo.PipelineInfoMain.Subpass = 0;

		// Load Vulkan functions for ImGui
		ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, [](const char* name, void* userData) -> PFN_vkVoidFunction {
			auto* dev = static_cast<VulkanDevice*>(userData);
			return dev->GetProcAddr(name);
		}, vkDev);

		ImGui_ImplVulkan_Init(&initInfo);
	}

	void ImGuiLayer::ShutdownVulkanBackend()
	{
		ImGui_ImplVulkan_Shutdown();

		auto* gfxCtx = static_cast<VulkanGraphicsContext*>(
			static_cast<WindowsWindow*>(&Application::Get().GetWindow())->GetGraphicsContext());
		if (gfxCtx && gfxCtx->GetDevice() && m_VkDescriptorPool)
		{
			gfxCtx->GetDevice()->fnDestroyDescriptorPool(
				gfxCtx->GetDevice()->GetVkDevice(),
				static_cast<VkDescriptorPool>(m_VkDescriptorPool), nullptr);
		}
		m_VkDescriptorPool = nullptr;
	}

	void ImGuiLayer::NewFrameVulkan()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
	}

	void ImGuiLayer::RenderVulkan(ImDrawData* drawData)
	{
		if (!drawData || drawData->CmdListsCount == 0) return;
		ImGui_ImplVulkan_RenderDrawData(drawData, VK_NULL_HANDLE, VK_NULL_HANDLE);
	}

#endif // CANDY_PLATFORM_WINDOWS

}
