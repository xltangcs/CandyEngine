#pragma once

#include "Candy.h"
#include "Runtime/Project/RecentProjects.h"
#include "Setting/EditorSettings.h"
#include "Setting/EditorState.h"
#include "Panel/SceneHierarchyPanel.h"
#include "Panel/ContentBrowserPanel.h"
#include "Panel/EditorSettingsPanel.h"
#include "Panel/ProjectSettingsPanel.h"
#include "Runtime/Renderer/EditorCamera.h"

// Enable DX12 triangle rendering test
#define CANDY_DX12_TEST 1

#if CANDY_DX12_TEST
struct GLFWwindow;
namespace Candy {
	class DX12Device;
	class DX12SwapChain;
}
#endif

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

		void OnOverlayRender();

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

		void RenderPreviewScene();

		// Project
		void OpenRecent(const std::filesystem::path& path);
		void UI_BuildDialog();

#if CANDY_DX12_TEST
		// DX12 test: renders a colored triangle in a separate window
		void InitDX12Test();
		void RenderDX12Triangle();
		void ShutdownDX12Test();
#endif
	
	private:
		Candy::OrthographicCameraController m_CameraController;

		// Temp
		Ref<VertexArray> m_SquareVA;
		Ref<Shader> m_FlatColorShader;
		Ref<Framebuffer> m_Framebuffer;

		// Camera Preview (PIP)
		Entity m_CameraPreviewEntity;
		bool m_CameraPreviewPinned = false;
		Ref<Framebuffer> m_CameraPreviewFramebuffer;
		float m_CameraPreviewSize = 0.3f;

		Ref<Scene> m_ActiveScene;
		Ref<Scene> m_EditorScene;
		std::filesystem::path m_EditorScenePath;

		Entity m_SquareEntity;

		Entity m_CameraEntity;
		Entity m_SecondCamera;
		Entity m_HoveredEntity;

		EditorCamera m_EditorCamera;

		Ref<Texture2D> m_CheckerboardTexture;
		bool m_ViewportFocused = false, m_ViewportHovered = false;
		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		glm::vec2 m_ViewportBounds[2];
		glm::vec4 m_SquareColor = { 0.2f, 0.3f, 0.8f, 1.0f };
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

#if CANDY_DX12_TEST
		// DX12 test state
		GLFWwindow*                  m_DX12TestWindow = nullptr;
		Scope<DX12Device>            m_DX12Device;
		Ref<RHISwapChain>            m_DX12SwapChain;
		Ref<RHIBuffer>               m_DX12VertexBuffer;
		Ref<RHIBuffer>               m_DX12MVPBuffer;
		Ref<RHIShaderModule>         m_DX12VS;
		Ref<RHIShaderModule>         m_DX12PS;
		Ref<RHIGraphicsPipeline>     m_DX12Pipeline;
		bool                         m_DX12Initialized = false;
#endif
	};

}