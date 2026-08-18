#pragma once

#include "Candy.h"
#include "Runtime/Project/RecentProjects.h"
#include "Setting/EditorSettings.h"
#include "Setting/EditorState.h"
#include "Setting/LayoutPresetManager.h"
#include "Panel/SceneHierarchyPanel.h"
#include "Panel/ContentBrowserPanel.h"
#include "Panel/EditorSettingsPanel.h"
#include "Panel/ProjectSettingsPanel.h"
#include "Runtime/Renderer/EditorCamera.h"

namespace Candy {

	class EditorLayer : public Layer
	{
	public:
		EditorLayer();
		virtual ~EditorLayer() override;

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		void OnUpdate(Timestep ts) override;
		virtual void OnImGuiRender() override;
		void OnEvent(Event& e) override;

	private:
		bool OnKeyPressed(KeyPressedEvent& e);
		bool OnMouseButtonPressed(MouseButtonPressedEvent& e);

		void NewScene();
		void OpenScene();
		void OpenScene(const std::filesystem::path& path);
		void SaveScene();
		void SaveSceneAs();

		void SerializeScene(Ref<Scene> scene, const std::filesystem::path& path);
		void OnScenePlay();
		void OnSceneSimulate();
		void OnSceneStop();

		void OnDuplicateEntity();

		void FocusEntity(Entity entity);
		float CalculateEntityHalfExtent(Entity entity) const;

		// Project
		void OpenRecent(const std::filesystem::path& path);
		void UI_BuildDialog();
		void UI_LayoutDialogs();
	
	private:
		Candy::OrthographicCameraController m_CameraController;

		Ref<Framebuffer> m_Framebuffer;

		// Camera Preview (PIP)
		Entity m_CameraPreviewEntity;
		bool m_CameraPreviewPinned = false;
		Ref<Framebuffer> m_CameraPreviewFramebuffer;
		float m_CameraPreviewSize = 0.3f;

		Ref<Scene> m_ActiveScene;
		Ref<Scene> m_EditorScene;
		std::filesystem::path m_EditorScenePath;

		Entity m_HoveredEntity;

		EditorCamera m_EditorCamera;

		Ref<Texture2D> m_CheckerboardTexture;
		bool m_ViewportFocused = false, m_ViewportHovered = false;
		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		glm::vec2 m_ViewportBounds[2];
		int m_GizmoType = -1;

		enum class SceneState
		{
			Edit = 0, Play = 1, Simulate = 2
		};
		SceneState m_SceneState = SceneState::Edit;

		// Panels
		SceneHierarchyPanel m_SceneHierarchyPanel;
		ContentBrowserPanel m_ContentBrowserPanel;

		// Editor resources
		Ref<Texture2D> m_IconPlay, m_IconStop, m_IconSimulate;

		bool m_SceneDirty = false;
		std::vector<RecentProjectEntry> m_RecentProjects;

		void BuildGame_Full();
		void BuildGame_ContentOnly();

		// Build / package dialog state
		bool m_ShowBuildDialog = false;
		int m_BuildMode = 0; // 0 = Content Only, 1 = Full Build (MSBuild)
		int m_BuildConfig = 0; // 0 = Debug, 1 = Release, 2 = Dist

		// Editor Layout preset state
		bool m_LayoutInitialized = false;  // prevents re-applying the first-run default
		bool m_ShowSaveLayoutDialog = false;
		char m_NewLayoutName[128] = { 0 };
		std::string m_CurrentLayoutName;   // preset currently in use (empty = Default); drives the ">" marker

		// Deferred layout request. Dock tree rebuilds must run BEFORE the DockSpace
		// submission (imgui DockBuilder contract), so menu clicks only queue them.
		enum class LayoutRequestType { None, ApplyDefault, LoadPreset };
		LayoutRequestType m_PendingLayoutRequest = LayoutRequestType::None;
		std::string m_PendingLayoutPresetName;
	};

}
