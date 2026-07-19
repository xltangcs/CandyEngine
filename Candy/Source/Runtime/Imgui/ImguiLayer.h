#pragma once

#include "Runtime/Core/Layer.h"
#include "Runtime/Events/ApplicationEvent.h"
#include "Runtime/Events/KeyEvent.h"
#include "Runtime/Events/MouseEvent.h"

struct ImGuiContext;
struct ImDrawData;
struct ImGuiIO;

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
		// Call after VFS .pak is mounted (OnAttach may run before .pak mount in standalone mode).
		void ReloadFontsFromVfs();

		// Disable the editor context's chrome in standalone game mode:
		// clears any ini-loaded window layout and stops loading/saving imgui.ini,
		// so Pass 3 (editorContext) produces empty draw data and won't overlay the game UI.
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
	};

}