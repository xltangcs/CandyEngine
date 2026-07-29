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
#include "Platform/DX12/DX12GraphicsContext.h"
#include "Platform/Vulkan/VulkanGraphicsContext.h"
#include "Platform/Windows/WindowsWindow.h"
#endif

#ifndef CANDY_PLATFORM_WINDOWS
#include <glad/glad.h>
#include <backends/imgui_impl_opengl3.h>
#endif

namespace Candy {

	// =========================================================================
	// Backend detection
	// =========================================================================

	bool ImGuiLayer::IsDX12Backend() const
	{
		return m_IsDX12;
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
		m_IsDX12   = (Renderer::GetAPI() == RendererAPI::API::DX12);
		bool isVulkan = (Renderer::GetAPI() == RendererAPI::API::Vulkan);

		if (isVulkan)
		{
			// Vulkan: use GLFW for other (no OpenGL context), ImGui rendering via Vulkan backend
			// Full Vulkan ImGui integration (ImGui_ImplVulkan) will be implemented in a follow-up
			ImGui_ImplGlfw_InitForOther(window, true);
			CANDY_CORE_WARN("ImGuiLayer: Vulkan backend — ImGui_ImplVulkan not yet integrated, UI will not render");
		}
		else if (m_IsDX12)
		{
#ifdef CANDY_PLATFORM_WINDOWS
			// GLFW platform backend (no OpenGL context)
			ImGui_ImplGlfw_InitForOther(window, true);

			// DX12 renderer backend
			InitDX12Backend(window);

			CANDY_CORE_INFO("ImGuiLayer: DX12 backend initialized");
#endif
		}
		else
		{
#ifndef CANDY_PLATFORM_WINDOWS
			ImGui_ImplGlfw_InitForOpenGL(window, true);
			ImGui_ImplOpenGL3_Init("#version 410");
#endif
		}

		// Create game UI context
		m_GameUIContext = ImGui::CreateContext();
		ImGui::SetCurrentContext(m_GameUIContext);

		if (m_IsDX12)
		{
#ifdef CANDY_PLATFORM_WINDOWS
			m_DX12.GameUIContext = m_GameUIContext;
#endif
		}
		else
		{
#ifndef CANDY_PLATFORM_WINDOWS
			ImGui_ImplOpenGL3_Init("#version 410");
#endif
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
		if (m_IsDX12)
		{
#ifdef CANDY_PLATFORM_WINDOWS
			ImGui::SetCurrentContext(m_GameUIContext);
			ImGui_ImplDX12_Shutdown();
			ImGui::SetCurrentContext(m_EditorContext);
			ImGui_ImplDX12_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ShutdownDX12Backend();
#endif
		}
		else
		{
#ifndef CANDY_PLATFORM_WINDOWS
			ImGui::SetCurrentContext(m_GameUIContext);
			ImGui_ImplOpenGL3_Shutdown();
			ImGui::SetCurrentContext(m_EditorContext);
			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplGlfw_Shutdown();
#endif
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

		if (Renderer::GetAPI() == RendererAPI::API::Vulkan)
		{
			// Vulkan: GLFW new frame only (no ImGui rendering backend yet)
			ImGui_ImplGlfw_NewFrame();
		}
		else if (m_IsDX12)
		{
#ifdef CANDY_PLATFORM_WINDOWS
			NewFrameDX12();
#endif
		}
		else
		{
#ifndef CANDY_PLATFORM_WINDOWS
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
#endif
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

		if (Renderer::GetAPI() == RendererAPI::API::Vulkan)
		{
			// Vulkan: ImGui rendering not yet integrated — skip
		}
		else if (m_IsDX12)
		{
#ifdef CANDY_PLATFORM_WINDOWS
			RenderDX12(ImGui::GetDrawData());
#endif
		}
		else
		{
#ifndef CANDY_PLATFORM_WINDOWS
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
		}

		if (!m_IsDX12 && (Renderer::GetAPI() != RendererAPI::API::Vulkan)
		    && (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
		{
#ifndef CANDY_PLATFORM_WINDOWS
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
#endif
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

		if (m_IsDX12)
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

		if (m_IsDX12)
		{
#ifdef CANDY_PLATFORM_WINDOWS
			RenderDX12(ImGui::GetDrawData());
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
	// DX12 Backend Implementation (Windows only)
	// =========================================================================

#ifdef CANDY_PLATFORM_WINDOWS

	void ImGuiLayer::SRVAllocator(ImGui_ImplDX12_InitInfo* info,
	                              D3D12_CPU_DESCRIPTOR_HANDLE* outCPU,
	                              D3D12_GPU_DESCRIPTOR_HANDLE* outGPU)
	{
		auto* self = static_cast<ImGuiLayer*>(info->UserData);
		auto& s = self->m_DX12;

		// Linear allocation from the SRV heap (simple, resets each frame)
		D3D12_CPU_DESCRIPTOR_HANDLE cpu = s.SRVHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE gpu = s.SRVHeap->GetGPUDescriptorHandleForHeapStart();

		cpu.ptr += static_cast<SIZE_T>(s.SRVHeapUsed) * s.SRVDescSize;
		gpu.ptr += static_cast<SIZE_T>(s.SRVHeapUsed) * s.SRVDescSize;

		s.SRVHeapUsed++;

		*outCPU = cpu;
		*outGPU = gpu;
	}

	void ImGuiLayer::InitDX12Backend(GLFWwindow* window)
	{
		// Get DX12 device/queue from GraphicsContext
		auto* win = static_cast<WindowsWindow*>(&Application::Get().GetWindow());
		auto* gfxCtx = static_cast<DX12GraphicsContext*>(
			win->GetGraphicsContext());

		m_DX12.Device = gfxCtx->GetDevice()->GetNativeDevice();
		m_DX12.Queue  = gfxCtx->GetDevice()->GetNativeQueue();

		// Create SRV descriptor heap for ImGui textures
		{
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
			heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.NumDescriptors = 16;
			heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			HRESULT hr = m_DX12.Device->CreateDescriptorHeap(
				&heapDesc, IID_PPV_ARGS(&m_DX12.SRVHeap));
			if (FAILED(hr))
			{
				CANDY_CORE_ERROR("ImGuiLayer::InitDX12Backend: SRV heap creation failed");
				return;
			}

			m_DX12.SRVDescSize = m_DX12.Device->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}

		// Create per-frame resources (2 frames for double-buffering)
		for (int i = 0; i < 2; ++i)
		{
			HRESULT hr = m_DX12.Device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(&m_DX12.FrameAllocators[i]));
			if (FAILED(hr))
			{
				CANDY_CORE_ERROR("ImGuiLayer: CreateCommandAllocator[{0}] failed", i);
				return;
			}

			hr = m_DX12.Device->CreateCommandList(
				0, D3D12_COMMAND_LIST_TYPE_DIRECT,
				m_DX12.FrameAllocators[i], nullptr,
				IID_PPV_ARGS(&m_DX12.FrameCmdLists[i]));
			if (FAILED(hr))
			{
				CANDY_CORE_ERROR("ImGuiLayer: CreateCommandList[{0}] failed", i);
				return;
			}

			m_DX12.FrameCmdLists[i]->Close();
		}

		// Fence for synchronization
		m_DX12.Device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		                           IID_PPV_ARGS(&m_DX12.Fence));
		m_DX12.FenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);

		// Initialize ImGui DX12 backend
		ImGui_ImplDX12_InitInfo initInfo = {};
		initInfo.Device               = m_DX12.Device;
		initInfo.CommandQueue          = m_DX12.Queue;
		initInfo.NumFramesInFlight     = 2;
		initInfo.RTVFormat            = DXGI_FORMAT_B8G8R8A8_UNORM;
		initInfo.DSVFormat            = DXGI_FORMAT_UNKNOWN;
		initInfo.SrvDescriptorHeap    = m_DX12.SRVHeap;
		initInfo.SrvDescriptorAllocFn = SRVAllocator;
		initInfo.SrvDescriptorFreeFn  = nullptr;  // linear allocator, no free needed
		initInfo.UserData             = this;

		if (!ImGui_ImplDX12_Init(&initInfo))
		{
			CANDY_CORE_ERROR("ImGuiLayer: ImGui_ImplDX12_Init failed");
		}
	}

	void ImGuiLayer::ShutdownDX12Backend()
	{
		// Wait for GPU to finish
		if (m_DX12.Fence && m_DX12.Queue)
		{
			m_DX12.FenceValue++;
			m_DX12.Queue->Signal(m_DX12.Fence, m_DX12.FenceValue);
			if (m_DX12.Fence->GetCompletedValue() < m_DX12.FenceValue)
			{
				m_DX12.Fence->SetEventOnCompletion(m_DX12.FenceValue, m_DX12.FenceEvent);
				WaitForSingleObject(m_DX12.FenceEvent, INFINITE);
			}
		}

		for (int i = 0; i < 2; ++i)
		{
			if (m_DX12.FrameCmdLists[i])   m_DX12.FrameCmdLists[i]->Release();
			if (m_DX12.FrameAllocators[i]) m_DX12.FrameAllocators[i]->Release();
		}
		if (m_DX12.SRVHeap) m_DX12.SRVHeap->Release();
		if (m_DX12.Fence)   m_DX12.Fence->Release();

		if (m_DX12.FenceEvent)
		{
			CloseHandle(m_DX12.FenceEvent);
			m_DX12.FenceEvent = nullptr;
		}

		memset(&m_DX12, 0, sizeof(m_DX12));
	}

	void ImGuiLayer::NewFrameDX12()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplGlfw_NewFrame();
	}

	void ImGuiLayer::RenderDX12(ImDrawData* drawData)
	{
		if (!drawData || drawData->CmdListsCount == 0)
			return;

		uint32_t fi = m_DX12.FrameIndex;

		// Wait for previous frame to complete
		if (m_DX12.Fence->GetCompletedValue() < m_DX12.FenceValue)
		{
			m_DX12.Fence->SetEventOnCompletion(m_DX12.FenceValue, m_DX12.FenceEvent);
			WaitForSingleObject(m_DX12.FenceEvent, INFINITE);
		}

		// Reset allocator and command list
		m_DX12.FrameAllocators[fi]->Reset();
		m_DX12.FrameCmdLists[fi]->Reset(m_DX12.FrameAllocators[fi], nullptr);

		// Set descriptor heaps
		ID3D12DescriptorHeap* heaps[] = { m_DX12.SRVHeap };
		m_DX12.FrameCmdLists[fi]->SetDescriptorHeaps(1, heaps);

		// Render ImGui draw data
		ImGui_ImplDX12_RenderDrawData(drawData, m_DX12.FrameCmdLists[fi]);

		m_DX12.FrameCmdLists[fi]->Close();

		// Submit
		ID3D12CommandList* lists[] = { m_DX12.FrameCmdLists[fi] };
		m_DX12.Queue->ExecuteCommandLists(1, lists);

		// Signal fence for this frame
		m_DX12.FenceValue++;
		m_DX12.Queue->Signal(m_DX12.Fence, m_DX12.FenceValue);

		// Present the DX12 swap chain
		auto* gfxCtx = static_cast<DX12GraphicsContext*>(
			static_cast<WindowsWindow*>(&Application::Get().GetWindow())
				->GetGraphicsContext());

		if (auto* sc = gfxCtx->GetSwapChain())
		{
			UINT flags = 0;
			sc->GetSwapChain()->Present(1, flags);
			sc->AdvanceFrame();
		}

		// Advance frame index (double-buffered)
		m_DX12.FrameIndex = (fi + 1) % 2;

		// Reset SRV descriptor allocator
		m_DX12.SRVHeapUsed = 0;
	}

#endif // CANDY_PLATFORM_WINDOWS

}
