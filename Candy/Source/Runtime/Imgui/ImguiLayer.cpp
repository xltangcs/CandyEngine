#include "CandyPCH.h"


#include "Runtime/Core/Application.h"
#include "Runtime/Core/FileSystem.h"

#include "Runtime/Imgui/ImguiLayer.h"

#include <imgui.h> 
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include "ImGuizmo.h"
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <filesystem>

#include "imgui_internal.h"

namespace Candy {

	ImGuiLayer::ImGuiLayer()
		: Layer("ImGuiLayer")
	{
	}

	void ImGuiLayer::OnAttach()
	{
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		m_EditorContext = ImGui::GetCurrentContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
		//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
		//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;

		std::filesystem::create_directories("Saved");
		io.IniFilename = "Saved/imgui.ini";

		// Load fonts from VFS (may fail in standalone mode if .pak not yet mounted;
		// ReloadFontsFromVfs() is called after .pak mount to retry)
		LoadFontsFromVfs(io);

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();

		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		SetDarkThemeColors();

		Application& app = Application::Get();
		GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());

		// Setup Platform/Renderer bindings
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 410");

		// Create separate game UI context with own atlas (no docking, no viewports, no ini file)
		m_GameUIContext = ImGui::CreateContext();
		ImGui::SetCurrentContext(m_GameUIContext);
		ImGui_ImplOpenGL3_Init("#version 410");
		ImGuiIO& gameIO = ImGui::GetIO();
		gameIO.IniFilename = nullptr;
		gameIO.ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;
		gameIO.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
		gameIO.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
		LoadFontsFromVfs(gameIO);
		ImGui::SetCurrentContext(m_EditorContext); // Restore editor context
	}

	void ImGuiLayer::OnDetach()
	{
		ImGui::SetCurrentContext(m_GameUIContext);
		ImGui_ImplOpenGL3_Shutdown();
		ImGui::SetCurrentContext(m_EditorContext);
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext(m_GameUIContext);
		ImGui::DestroyContext(m_EditorContext);
	}

	void ImGuiLayer::OnEvent(Event& e)
	{
		if (m_BlockEvents)
		{
			ImGuiIO& io = ImGui::GetIO();
			e.Handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
			e.Handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
		}
	}

	void ImGuiLayer::Begin()
	{
		if (m_EditorChromeDisabled)
			return;
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
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
		// Rendering
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
	}

	void ImGuiLayer::BeginGameUI(float displayW, float displayH, float mouseX, float mouseY, bool mouseDown, float deltaTime)
	{
		ImGui::SetCurrentContext(m_GameUIContext);
		ImGuiIO& io = ImGui::GetIO();

		io.DisplaySize = ImVec2(displayW, displayH);
		io.DeltaTime = deltaTime;
		io.MousePos = ImVec2(mouseX, mouseY);
		io.MouseDown[0] = mouseDown;

		ImGui_ImplOpenGL3_NewFrame();
		// Note: no ImGui_ImplGlfw_NewFrame() -- we manually feed input
		// Note: no ImGuizmo::BeginFrame() -- game UI doesn't use gizmo
		ImGui::NewFrame();
	}

	void ImGuiLayer::EndGameUI()
	{
		ImGui::SetCurrentContext(m_GameUIContext);
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		// Note: no UpdatePlatformWindows / RenderPlatformWindowsDefault (viewports disabled)
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
		// Editor context
		ImGui::SetCurrentContext(m_EditorContext);
		ImGuiIO& editorIO = ImGui::GetIO();
		editorIO.Fonts->Clear();
		LoadFontsFromVfs(editorIO);

		// Game UI context
		if (m_GameUIContext)
		{
			ImGui::SetCurrentContext(m_GameUIContext);
			ImGuiIO& gameIO = ImGui::GetIO();
			gameIO.Fonts->Clear();
			LoadFontsFromVfs(gameIO);
		}

		// Note: this imgui version (2025-06-11+) uses dynamic font atlas
		// (ImGuiBackendFlags_RendererHasTextures). Font textures are automatically
		// rebuilt on the next RenderDrawData -- no manual CreateFontsTexture needed.
		ImGui::SetCurrentContext(m_EditorContext);
	}

	void ImGuiLayer::DisableEditorChrome()
	{
		m_EditorChromeDisabled = true;
		ImGui::SetCurrentContext(m_EditorContext);
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = nullptr;            // stop loading/saving imgui.ini
		ImGui::ClearIniSettings();           // drop any window layout loaded from Saved/imgui.ini
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

		// Headers
		colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Buttons
		colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Frame BG
		colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Tabs
		colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
		colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
		colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };

		// Title
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	}

}
