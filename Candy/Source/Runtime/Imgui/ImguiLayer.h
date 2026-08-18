#pragma once

#include "Runtime/Core/Layer.h"
#include "Runtime/Events/ApplicationEvent.h"
#include "Runtime/Events/KeyEvent.h"
#include "Runtime/Events/MouseEvent.h"

struct ImGuiContext;
struct ImDrawData;
struct ImGuiIO;

namespace Candy {

	class Framebuffer;
	class ImGuiBackend;

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
		/// End the game UI frame. `target` (if given) is the viewport framebuffer
		/// the game UI is composited into; if null, falls back to the swap chain.
		/// The game UI must render INTO the viewport, NOT present the swap chain —
		/// presenting it separately from the editor's own present flips the swap
		/// chain twice per frame and hangs the GPU (device removed).
		void EndGameUI(Framebuffer* target = nullptr);
		ImDrawData* GetGameUIDrawData();
		bool GameUIWantsMouse() const;
	private:
		bool m_BlockEvents = true;
		float m_Time = 0.0f;

		// Disk path backing io.IniFilename ("VFS://Engine/Saved/imgui.ini" resolved
		// via FileSystem). ImGui keeps the const char* pointer, so it must outlive
		// the context.
		std::string m_IniPath;

		ImGuiContext* m_EditorContext = nullptr;
		ImGuiContext* m_GameUIContext = nullptr;

		// Graphics-API adapter (OpenGL / D3D12 / Vulkan) — owns ALL
		// backend-specific state and rendering. This layer stays backend-agnostic.
		Scope<ImGuiBackend> m_Backend;

		void LoadFontsFromVfs(ImGuiIO& targetIO);
	};

}
