#include "CandyPCH.h"

#include "Runtime/Core/Application.h"
#include "Runtime/Core/FileSystem.h"
#include "Runtime/Renderer/Renderer.h"

#include "Runtime/Imgui/ImguiLayer.h"
#include "Runtime/Imgui/ImGuiBackend.h"

#include <imgui.h>
#include "ImGuizmo.h"
#include <GLFW/glfw3.h>
#include <filesystem>

#include "imgui_internal.h"

namespace Candy {

	// =========================================================================
	// Construction / destruction
	// =========================================================================

	ImGuiLayer::ImGuiLayer()
	{
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

		// Graphics-API backend adapter 鈥?owns all backend-specific rendering.
		m_Backend = ImGuiBackend::Create();

		// Multi-viewport platform rendering is only wired through the
		// OpenGL/Glfw backend (UpdatePlatformWindowsDefault path).  Backends
		// without platform-window support must disable the flag, otherwise
		// imgui.cpp trips the `Forgot to call UpdatePlatformWindows()` assertion.
		if (!m_Backend->SupportsPlatformWindows())
			io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

		// One-time init (GLFW platform + shared renderer resources), then
		// per-context renderer init for the editor context (current).
		m_Backend->Init(window);
		m_Backend->InitContext();
		CANDY_CORE_INFO("ImGuiLayer: {} backend initialized",
		                RendererAPI::StringFromAPI(Renderer::GetAPI()));

		// Create game UI context
		m_GameUIContext = ImGui::CreateContext();
		ImGui::SetCurrentContext(m_GameUIContext);

		// Per-context renderer init for the game UI context.
		m_Backend->InitContext();

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
		ImGui::SetCurrentContext(m_GameUIContext);
		m_Backend->ShutdownContext();
		ImGui::SetCurrentContext(m_EditorContext);
		m_Backend->ShutdownContext();
		m_Backend->Shutdown();

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

		m_Backend->NewFrame();

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

		// Swap-chain render + present (backend handles platform windows when supported).
		m_Backend->RenderDrawData(ImGui::GetDrawData(), nullptr);
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

		m_Backend->NewFrameGameUI();

		ImGui::NewFrame();
	}

	void ImGuiLayer::EndGameUI(Framebuffer* target)
	{
		ImGui::SetCurrentContext(m_GameUIContext);
		ImGui::Render();

		// The game UI belongs INSIDE the viewport: the backend composites it into
		// the target framebuffer (no present). When no target is available
		// (standalone game), it falls back to the swap chain.
		m_Backend->RenderDrawData(ImGui::GetDrawData(), target);

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
		auto boldData = FileSystem::Get().Read("VFS://Engine/Content/Fonts/opensans/OpenSans-Bold.ttf");
		if (boldData && !boldData->empty())
		{
			void* fontMem = ImGui::MemAlloc(boldData->size());
			memcpy(fontMem, boldData->data(), boldData->size());
			targetIO.Fonts->AddFontFromMemoryTTF(fontMem, (int)boldData->size(), 18.0f);
		}

		auto regularData = FileSystem::Get().Read("VFS://Engine/Content/Fonts/opensans/OpenSans-Regular.ttf");
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

}
