#include "EditorLayer.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <sstream>
#include <cstdio>
#include <vector>
#include <fstream>

#include "Runtime/Scene/SceneSerializer.h"
#include "Runtime/Utils/PlatformUtils.h"
#include "Runtime/UI/UISystem.h"
#include "Runtime/Renderer/GameFrameRenderer.h"

#include <imgui/misc/cpp/imgui_stdlib.h>
#include "ImGuizmo.h"

#include "Runtime/Math/Math.h"

#include <GLFW/glfw3.h>

#include "Runtime/Project/ProjectUtils.h"
#include "Runtime/Core/PakFile.h"
#include "Layer/ProjectManagerLayer.h"

#include <cstdlib>

#include "Runtime/Core/FileSystem.h"

namespace Candy {

	EditorLayer::EditorLayer()
		: Layer("EditorLayer"), m_CameraController(1280.0f / 720.0f), m_SquareColor({ 0.2f, 0.3f, 0.8f, 1.0f })
	{
	}

	EditorLayer::~EditorLayer()
	{
	}

	void EditorLayer::OnAttach()
	{
		CANDY_PROFILE_FUNCTION();

		m_CheckerboardTexture = Texture2D::Create("VFS://Engine/Textures/Checkerboard.png");
		m_IconPlay = Texture2D::Create("VFS://Engine/Icons/PlayButton.png");
		m_IconStop = Texture2D::Create("VFS://Engine/Icons/StopButton.png");
		m_IconSimulate = Texture2D::Create("VFS://Engine/Icons/SimulateButton.png");

		FramebufferSpecification fbSpec;
		fbSpec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER, FramebufferTextureFormat::Depth };
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		m_Framebuffer = Framebuffer::Create(fbSpec);

		FramebufferSpecification previewFbSpec;
		previewFbSpec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::Depth };
		previewFbSpec.Width = 480;
		previewFbSpec.Height = 270;
		m_CameraPreviewFramebuffer = Framebuffer::Create(previewFbSpec);

		m_EditorScene = CreateRef<Scene>();
		m_ActiveScene = m_EditorScene;

		m_EditorCamera = EditorCamera(30.0f, 1.778f, 0.1f, 1000.0f);

		auto& editorSettings = EditorSettings::Get();
		editorSettings.Load();
		ImGuiLayer::RebuildFont(Application::Get().GetFontPath());

		m_RecentProjects = RecentProjects::Load();

		auto& editorState = EditorState::Get();
		editorState.Load();

		auto project = Application::Get().GetProject();
		if (project)
		{
			ProjectSettings::Get().Load();
			auto scenePathOpt = FileSystem::Get().ToDiskPath(ProjectSettings::Get().DefaultScene);
			if (scenePathOpt && std::filesystem::exists(*scenePathOpt))
				OpenScene(*scenePathOpt);
		}
		else if (!editorState.LastScenePath.empty() && std::filesystem::exists(editorState.LastScenePath))
		{
			OpenScene(editorState.LastScenePath);
		}

		m_SceneHierarchyPanel.SetContext(m_ActiveScene);

		// Apply persisted window size
		GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		if (editorState.WindowMaximized)
			glfwMaximizeWindow(window);
		else
			glfwSetWindowSize(window, editorState.WindowWidth, editorState.WindowHeight);
	}

	void EditorLayer::OnDetach()
	{
		CANDY_PROFILE_FUNCTION();
		EditorSettings::Get().Save();
		ProjectSettings::Get().Save();
		EditorState::Get().Save();
	}

	void EditorLayer::OnUpdate(Timestep ts)
	{
		CANDY_PROFILE_FUNCTION();

		// Resize
		if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
			m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f && // zero sized framebuffer is invalid
			(spec.Width != m_ViewportSize.x || spec.Height != m_ViewportSize.y))
		{
			m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_CameraController.OnResize(m_ViewportSize.x, m_ViewportSize.y);
			m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
			m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		}
		// Update
		if (m_ViewportFocused)
			m_CameraController.OnUpdate(ts);
		m_EditorCamera.OnUpdate(ts);

		Candy::Renderer2D::ResetStats();

		m_Framebuffer->Bind();

		// Clear color+depth FIRST, then clear entity-ID attachment to -1.
		// Order matters: glClear wipes all draw buffers (including RED_INTEGER) with the
		// float clear color, which would overwrite -1 with 0 and break entity picking.
		Candy::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		Candy::RenderCommand::Clear();
		m_Framebuffer->ClearAttachment(1, -1);

		// Render scene via GameFrameRenderer
		switch (m_SceneState)
		{
			case SceneState::Edit:
			{
				if (m_ViewportFocused)
					m_CameraController.OnUpdate(ts);

				m_EditorCamera.OnUpdate(ts);

				GameFrameRenderer::RenderSceneTo(*m_Framebuffer, *m_ActiveScene, &m_EditorCamera);
				break;
			}
			case SceneState::Simulate:
			{
				m_EditorCamera.OnUpdate(ts);

				m_ActiveScene->OnUpdateSimulationLogic(ts);
				GameFrameRenderer::RenderSceneTo(*m_Framebuffer, *m_ActiveScene, &m_EditorCamera);
				break;
			}
			case SceneState::Play:
			{
				m_ActiveScene->OnUpdateRuntimeLogic(ts);
				GameFrameRenderer::RenderSceneTo(*m_Framebuffer, *m_ActiveScene, nullptr);
				break;
			}
		}

		auto [mx, my] = ImGui::GetMousePos();
		mx -= m_ViewportBounds[0].x;
		my -= m_ViewportBounds[0].y;
		glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
		// ImGui IO.MousePos needs left-top origin (unflipped my);
		// ReadPixel needs left-bottom origin (flipped my).
		float uiMouseY = my;
		my = viewportSize.y - my;
		int mouseX = (int)mx;
		int mouseY = (int)my;

		if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y)
		{
			int pixelData = m_Framebuffer->ReadPixel(1, mouseX, mouseY);
			m_HoveredEntity = pixelData == -1 ? Entity() : Entity((entt::entity)pixelData, m_ActiveScene.get());
		}

		OnOverlayRender();

		// Camera Preview PIP
		{
			Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
			if (!m_CameraPreviewPinned)
			{
				if (selectedEntity && selectedEntity.HasComponent<CameraComponent>())
				{
					m_CameraPreviewEntity = selectedEntity;
				}
				else
				{
					m_CameraPreviewEntity = {};
				}
			}
			else if (!m_CameraPreviewEntity || !m_CameraPreviewEntity.HasComponent<CameraComponent>())
			{
				m_CameraPreviewEntity = {};
				m_CameraPreviewPinned = false;
			}
			if (m_CameraPreviewEntity)
			{
				RenderPreviewScene();
			}
		}

		// Render game UI into FBO
		{
			bool mouseDown = ImGui::GetIO().MouseDown[0];
			GameFrameRenderer::RenderUITo(*m_Framebuffer, *m_ActiveScene, mx, uiMouseY, mouseDown, ts.GetSeconds());
		}

		m_Framebuffer->Unbind();
	}

	void EditorLayer::OnImGuiRender()
	{
		CANDY_PROFILE_FUNCTION();
		
		// Note: Switch this to true to enable dockspace
		static bool dockspaceOpen = true;
		static bool opt_fullscreen_persistant = true;
		bool opt_fullscreen = opt_fullscreen_persistant;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		if (opt_fullscreen)
		{
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->Pos);
			ImGui::SetNextWindowSize(viewport->Size);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}

		// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive, 
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise 
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
		ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		// DockSpace
		ImGuiIO& io = ImGui::GetIO();

		ImGuiStyle& style = ImGui::GetStyle();
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 370.0f;

		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

		style.WindowMinSize.x = minWinSizeX;

		// ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 8));
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New Scene", "Ctrl+N"))
					NewScene();
				if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
					OpenScene();
				
				ImGui::Separator();
				
				if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
					SaveScene();

				if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
					SaveSceneAs();
				
				ImGui::Separator();
				
				if (ImGui::BeginMenu("Recent Projects"))
				{
					if (m_RecentProjects.empty())
					{
						ImGui::MenuItem("(none)", nullptr, false, false);
					}
					else
					{
						for (const auto& entry : m_RecentProjects)
						{
							if (ImGui::MenuItem(entry.Name.c_str()))
								OpenRecent(entry.Path);
						}
					}
					ImGui::EndMenu();
				}

				ImGui::Separator();
				
				if (ImGui::MenuItem("Open Project Manager...", "Ctrl+Shift+N"))
				{
					Application::Get().SchedulePopLayer(this);
					Application::Get().SchedulePushLayer(new ProjectManagerLayer());
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Exit")) Application::Get().Close();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Setting"))
			{
				if (ImGui::MenuItem("Project Settings"))
					EditorState::Get().ShowProjectSettings = true;

				if (ImGui::MenuItem("Editor Settings"))
					EditorState::Get().ShowEditorSettings = true;

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Build"))
			{
				if (ImGui::MenuItem("Build and Package..."))
					m_ShowBuildDialog = true;
				ImGui::EndMenu();
			}

			{
				bool enabled = (bool)m_ActiveScene;
				ImVec4 tint = ImVec4(1, 1, 1, 1);
				if (!enabled) tint.w = 0.5f;

				float size = ImGui::GetFrameHeight() - 4.0f;
				float tw = size * 2.0f + ImGui::GetStyle().ItemSpacing.x;
				float remaining = ImGui::GetWindowContentRegionMax().x - ImGui::GetCursorPosX();
				if (remaining > tw)
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (remaining - tw) * 0.5f);
			
				Ref<Texture2D> playIcon = (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Simulate) ? m_IconPlay : m_IconStop;
				if (ImGui::ImageButton("##PlayStop", (ImTextureID)playIcon->GetRendererID(), ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tint) && enabled)
				{
					if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Simulate)
						OnScenePlay();
					else if (m_SceneState == SceneState::Play)
						OnSceneStop();
				}
				ImGui::SameLine();
				Ref<Texture2D> simIcon = (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Play) ? m_IconSimulate : m_IconStop;
				if (ImGui::ImageButton("##Simulate", (ImTextureID)simIcon->GetRendererID(), ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tint) && enabled)
				{
					if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Play)
						OnSceneSimulate();
					else if (m_SceneState == SceneState::Simulate)
						OnSceneStop();
				}
			}

			ImGui::EndMenuBar();
		}
		// ImGui::PopStyleVar();
		
		m_SceneHierarchyPanel.OnImGuiRender();
		m_ContentBrowserPanel.OnImGuiRender();

		ImGui::Begin("Stats");

		std::string name = "None";
		if (m_HoveredEntity && m_ActiveScene)
		{
			auto& registry = m_ActiveScene->GetRegistry();
			entt::entity handle = (entt::entity)m_HoveredEntity;
			if (auto* tc = registry.try_get<TagComponent>(handle))
				name = tc->Tag;
		}
		ImGui::Text("Hovered Entity: %s", name.c_str());

		auto stats = Renderer2D::GetStats();
		ImGui::Text("Renderer2D Stats:");
		ImGui::Text("Draw Calls: %d", stats.DrawCalls);
		ImGui::Text("Quads: %d", stats.QuadCount);
		ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
		ImGui::Text("Indices: %d", stats.GetTotalIndexCount());

		ImGui::End();

		ImGui::Begin("Settings");
		ImGui::Checkbox("Show physics colliders", &m_ShowPhysicsColliders);
		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport");

		auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
		auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
		auto viewportOffset = ImGui::GetWindowPos();
		m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
		m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered();
		bool blockEvents = (!m_ViewportFocused && !m_ViewportHovered) && !io.WantCaptureKeyboard;
		Application::Get().GetImGuiLayer()->BlockEvents(blockEvents);

		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

		uint64_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
		ImGui::Image(reinterpret_cast<void*>(textureID), ImVec2{ m_ViewportSize.x, m_ViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
		
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				const char* path = (const char*)payload->Data;
				VfsPath vp = VfsPath::Parse(path);
				if (vp.IsValid())
				{
					std::filesystem::path relPath(vp.relativePath);
					if (relPath.extension() == ".candy")
					{
						auto disk = FileSystem::Get().ToDiskPath(vp);
						if (disk)
							OpenScene(*disk);
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		// Gizmos
		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		if (selectedEntity && m_GizmoType != -1)
		{
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist();

			ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y, m_ViewportBounds[1].x - m_ViewportBounds[0].x, m_ViewportBounds[1].y - m_ViewportBounds[0].y);
			
			// Editor camera
			const glm::mat4& cameraProjection = m_EditorCamera.GetProjection();
			glm::mat4 cameraView = m_EditorCamera.GetViewMatrix();

			// Entity transform
			auto& tc = selectedEntity.GetComponent<TransformComponent>();
			glm::mat4 transform = tc.GetTransform();

			// Snapping
			bool snap = Input::IsKeyPressed(Key::LeftControl);
			float snapValue = 0.5f; // Snap to 0.5m for translation/scale
			// Snap to 45 degrees for rotation
			if (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
				snapValue = 45.0f;

			float snapValues[3] = { snapValue, snapValue, snapValue };

			ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection),
				(ImGuizmo::OPERATION)m_GizmoType, ImGuizmo::LOCAL, glm::value_ptr(transform),
				nullptr, snap ? snapValues : nullptr);

			if (ImGuizmo::IsUsing())
			{
				glm::vec3 translation, rotation, scale;
				Math::DecomposeTransform(transform, translation, rotation, scale);

				glm::vec3 deltaRotation = rotation - tc.Rotation;
				tc.Translation = translation;
				tc.Rotation += deltaRotation;
				tc.Scale = scale;
			}
		}

		// Camera Preview PIP overlay
		if (m_CameraPreviewEntity)
		{
			float previewWidth = m_ViewportSize.x * m_CameraPreviewSize;
			float aspectRatio = 16.0f / 9.0f;
			float previewHeight = previewWidth / aspectRatio;
			float padding = 10.0f;

			ImVec2 previewPos(
				viewportPanelSize.x - previewWidth - padding,
				viewportPanelSize.y - previewHeight - padding
			);

			// Pin button + camera name
			ImGui::SetCursorPos(ImVec2(previewPos.x, previewPos.y - 22.0f));
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.7f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.8f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.8f));
			if (ImGui::SmallButton(m_CameraPreviewPinned ? "Unpin" : "Pin"))
			{
				m_CameraPreviewPinned = !m_CameraPreviewPinned;
			}
			ImGui::PopStyleColor(3);

			ImGui::SameLine();
			auto& tag = m_CameraPreviewEntity.GetComponent<TagComponent>();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.8f), tag.Tag.c_str());

			// Preview image
			ImGui::SetCursorPos(previewPos);
			uint64_t previewTextureID = m_CameraPreviewFramebuffer->GetColorAttachmentRendererID();
			ImGui::Image(
				reinterpret_cast<void*>(previewTextureID),
				ImVec2(previewWidth, previewHeight),
				ImVec2(0, 1), ImVec2(1, 0)
			);
		}

		ImGui::End();
		ImGui::PopStyleVar();

		if (EditorState::Get().ShowProjectSettings) ProjectSettingsPanel::OnImGuiRender();
		if (EditorState::Get().ShowEditorSettings) EditorSettingsPanel::OnImGuiRender();
		UI_BuildDialog();

		ImGui::End();
	}



	void EditorLayer::OnEvent(Event& e)
	{
		m_CameraController.OnEvent(e);
		m_EditorCamera.OnEvent(e);
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(CANDY_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
		dispatcher.Dispatch<MouseButtonPressedEvent>(CANDY_BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
	}

	bool EditorLayer::OnKeyPressed(KeyPressedEvent& e)
	{
		// Shortcuts
		if (e.GetRepeatCount() > 0)
			return false;

		bool control = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
		bool shift = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);
		switch (e.GetKeyCode())
		{
			case Key::N:
			{
				if (control && shift)
				{
					Application::Get().SchedulePopLayer(this);
					Application::Get().SchedulePushLayer(new ProjectManagerLayer());
				}
				else if (control)
					NewScene();

				break;
			}
			case Key::O:
			{
				if (control)
					OpenScene();

				break;
			}
			case Key::S:
			{
				if (control)
				{
					if (shift)
						SaveSceneAs();
					else
						SaveScene();
				}

				break;
			}
			// Scene Commands
			case Key::D:
			{
				if (control)
					OnDuplicateEntity();

				break;
			}

			// Gizmos
			case Key::Q:
			{
				if (!ImGuizmo::IsUsing())
					m_GizmoType = -1;
				break;
			}
			case Key::W:
			{
				if (!ImGuizmo::IsUsing())
					m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
				break;
			}
			case Key::E:
			{
				if (!ImGuizmo::IsUsing())
					m_GizmoType = ImGuizmo::OPERATION::ROTATE;
				break;
			}
			case Key::R:
			{
				if (!ImGuizmo::IsUsing())
					m_GizmoType = ImGuizmo::OPERATION::SCALE;
				break;
			}
		}
	}

	bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent& e)
	{
		if (e.GetMouseButton() == Mouse::ButtonLeft)
		{
			if (!m_HoveredEntity)
			{
				return false;
			}
			if (m_ViewportHovered && !ImGuizmo::IsOver() && !Input::IsKeyPressed(Key::LeftAlt))
			{
				m_SceneHierarchyPanel.SetSelectedEntity(m_HoveredEntity);
			}
		}
		return false;
	}

	void EditorLayer::OnOverlayRender()
	{
		if (m_SceneState == SceneState::Play)
		{
			Entity camera = m_ActiveScene->GetPrimaryCameraEntity();
			if (!camera)    return;
			Renderer2D::BeginScene(camera.GetComponent<CameraComponent>().Camera, camera.GetComponent<TransformComponent>().GetTransform());
		}
		else
		{
			Renderer2D::BeginScene(m_EditorCamera);
		}

		if (m_ShowPhysicsColliders)
		{
			// Box Colliders
			{
				auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>();
				for (auto entity : view)
				{
					auto [tc, bc2d] = view.get<TransformComponent, BoxCollider2DComponent>(entity);

					glm::vec3 translation = tc.Translation + glm::vec3(bc2d.Offset, 0.001f);
					glm::vec3 scale = tc.Scale * glm::vec3(bc2d.Size * 2.0f, 1.0f);

					glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
						* glm::rotate(glm::mat4(1.0f), tc.Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f))
						* glm::scale(glm::mat4(1.0f), scale);

					Renderer2D::DrawRect(transform, glm::vec4(0, 1, 0, 1));
				}
			}

			// Circle Colliders
			{
				auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>();
				for (auto entity : view)
				{
					auto [tc, cc2d] = view.get<TransformComponent, CircleCollider2DComponent>(entity);

					glm::vec3 translation = tc.Translation + glm::vec3(cc2d.Offset, 0.001f);
					glm::vec3 scale = tc.Scale * glm::vec3(cc2d.Radius * 2.0f);

					glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
						* glm::scale(glm::mat4(1.0f), scale);

					Renderer2D::DrawCircle(transform, glm::vec4(0, 1, 0, 1), 0.01f);
				}
			}
		}

		Renderer2D::EndScene();
	}

	void EditorLayer::NewScene()
	{
		m_ActiveScene = CreateRef<Scene>();
		m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
		m_EditorScenePath = std::filesystem::path();
		EditorState::Get().LastScenePath.clear();
		m_HoveredEntity = {};
		m_CameraPreviewEntity = {};
		m_CameraPreviewPinned = false;
	}

	void EditorLayer::OpenScene()
	{
		std::string filepath = FileDialogs::OpenFile("Candy Scene (*.candy)\0*.candy\0");
		if (!filepath.empty())
		{
			OpenScene(filepath);
		}
	}

	void EditorLayer::OpenScene(const std::filesystem::path& path)
	{

		if (m_SceneState != SceneState::Edit)
			OnSceneStop();
		if (path.extension().string() != ".candy")
		{
			CANDY_WARN("Could not load {0} - not a scene file", path.filename().string());
			return;
		}

		Ref<Scene> newScene = CreateRef<Scene>();
		SceneSerializer serializer(newScene);
		if (serializer.Deserialize(path.string()))
		{
			m_EditorScene = newScene;
			m_EditorScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_SceneHierarchyPanel.SetContext(m_EditorScene);

			m_ActiveScene = m_EditorScene;
			m_EditorScenePath = path;
			m_HoveredEntity = {};
			m_CameraPreviewEntity = {};
			m_CameraPreviewPinned = false;
			EditorState::Get().LastScenePath = path.string();
		}
	}

	void EditorLayer::SaveScene()
	{
		if (!m_EditorScenePath.empty())
			SerializeScene(m_ActiveScene, m_EditorScenePath);
		else
			SaveSceneAs();
	}

	void EditorLayer::SaveSceneAs()
	{
		std::string filepath = FileDialogs::SaveFile("Candy Scene (*.candy)\0*.candy\0");
		if (!filepath.empty())
		{
			SerializeScene(m_ActiveScene, filepath);
			m_EditorScenePath = filepath;
		}
	}

	void EditorLayer::SerializeScene(Ref<Scene> scene, const std::filesystem::path& path)
	{
		SceneSerializer serializer(scene);
		serializer.Serialize(path.string());
	}

	void EditorLayer::OnScenePlay()
	{
		if (m_SceneState == SceneState::Simulate)
			OnSceneStop();
		m_SceneState = SceneState::Play;
		m_ActiveScene = Scene::Copy(m_EditorScene);
		m_ActiveScene->OnRuntimeStart();

		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
		m_HoveredEntity = {};
		m_CameraPreviewEntity = {};
		m_CameraPreviewPinned = false;
	}

	void EditorLayer::OnSceneSimulate()
	{
		if (m_SceneState == SceneState::Play)
			OnSceneStop();

		m_SceneState = SceneState::Simulate;

		m_ActiveScene = Scene::Copy(m_EditorScene);
		m_ActiveScene->OnSimulationStart();

		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
		m_HoveredEntity = {};
		m_CameraPreviewEntity = {};
		m_CameraPreviewPinned = false;
	}

	void EditorLayer::OnSceneStop()
	{
		CANDY_CORE_ASSERT(m_SceneState == SceneState::Play || m_SceneState == SceneState::Simulate);

		if (m_SceneState == SceneState::Play)
			m_ActiveScene->OnRuntimeStop();
		else if (m_SceneState == SceneState::Simulate)
			m_ActiveScene->OnSimulationStop();
		m_SceneState = SceneState::Edit;

		m_ActiveScene = m_EditorScene;

		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
		m_HoveredEntity = {};
		m_CameraPreviewEntity = {};
		m_CameraPreviewPinned = false;
	}

	void EditorLayer::OnDuplicateEntity()
	{
		if (m_SceneState != SceneState::Edit)
			return;

		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		if (selectedEntity)
			m_EditorScene->DuplicateEntity(selectedEntity);
	}

	void EditorLayer::RenderPreviewScene()
	{
		auto& cameraComp = m_CameraPreviewEntity.GetComponent<CameraComponent>();
		auto& cameraTransform = m_CameraPreviewEntity.GetComponent<TransformComponent>();

		// Set camera viewport to match preview framebuffer size
		auto& previewSpec = m_CameraPreviewFramebuffer->GetSpecification();
		uint32_t prevWidth = previewSpec.Width;
		uint32_t prevHeight = previewSpec.Height;

		// Save and restore camera viewport size
		auto& sceneCamera = cameraComp.Camera;
		sceneCamera.SetViewportSize(prevWidth, prevHeight);

		m_CameraPreviewFramebuffer->Bind();
		RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		RenderCommand::Clear();

		m_ActiveScene->RenderSceneFromCamera(cameraComp, cameraTransform.GetTransform());

		m_CameraPreviewFramebuffer->Unbind();
		m_Framebuffer->Bind(); // Re-bind main FBO for subsequent UI rendering

		// Restore camera viewport to main viewport size
		sceneCamera.SetViewportSize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		
	}

	void EditorLayer::OpenRecent(const std::filesystem::path& path)
	{
		if (!std::filesystem::exists(path))
		{
			CANDY_CORE_WARN("Recent project no longer exists: {0}", path.string());
			return;
		}

		Application::Get().LoadProject(path);
		auto project = Application::Get().GetProject();
		if (project)
		{
			ProjectSettings::Get().Load();
			RecentProjects::Add(project->GetName(), project->GetProjectFileName().string());
			m_RecentProjects = RecentProjects::Load();

			auto scenePathOpt = FileSystem::Get().ToDiskPath(ProjectSettings::Get().DefaultScene);
			if (scenePathOpt && std::filesystem::exists(*scenePathOpt))
				OpenScene(*scenePathOpt);
		}
	}

	// Locate MSBuild.exe without relying on PATH (the editor may not be launched
	// from a VS Developer Command Prompt). Prefer vswhere, fall back to scanning
	// well-known Visual Studio installation directories.
	static std::string FindMSBuild()
	{
		// 1) vswhere -> MSBuild executable
		char pf86[512] = { 0 };
		size_t needed = 0;
		std::string pf86Str;
		if (getenv_s(&needed, pf86, sizeof(pf86), "ProgramFiles(x86)") == 0 && pf86[0] != '\0')
			pf86Str = pf86;
		else
			pf86Str = "C:\\Program Files (x86)";

		std::filesystem::path vswhere = std::filesystem::path(pf86Str) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
		if (std::filesystem::exists(vswhere))
		{
			std::string cmd = "\"";
			cmd += vswhere.string();
			cmd += "\" -latest -requires Microsoft.Component.MSBuild -find \"MSBuild\\**\\MSBuild.exe\" 2>nul";
			std::string out;
			FILE* pipe = _popen(cmd.c_str(), "r");
			if (pipe)
			{
				char buf[512];
				while (fgets(buf, sizeof(buf), pipe))
					out += buf;
				_pclose(pipe);
			}
			size_t start = out.find_first_not_of(" \t\r\n");
			if (start != std::string::npos)
			{
				size_t end = out.find_first_of("\r\n", start);
				std::string path = out.substr(start, end - start);
				if (!path.empty() && std::filesystem::exists(path))
					return path;
			}
		}

		// 2) Fallback: scan common VS 2019/2022 install layouts
		for (const char* edition : { "Community", "Professional", "Enterprise", "BuildTools" })
		{
			std::filesystem::path candidates[] = {
				std::filesystem::path(pf86Str) / "Microsoft Visual Studio" / "2022" / edition / "MSBuild" / "Current" / "Bin" / "MSBuild.exe",
				std::filesystem::path("C:\\Program Files\\Microsoft Visual Studio") / "2022" / edition / "MSBuild" / "Current" / "Bin" / "MSBuild.exe",
				std::filesystem::path(pf86Str) / "Microsoft Visual Studio" / "2019" / edition / "MSBuild" / "Current" / "Bin" / "MSBuild.exe",
				std::filesystem::path("C:\\Program Files\\Microsoft Visual Studio") / "2019" / edition / "MSBuild" / "Current" / "Bin" / "MSBuild.exe",
			};
			for (const auto& p : candidates)
				if (std::filesystem::exists(p))
					return p.string();
		}

		return std::string();
	}

	void EditorLayer::UI_BuildDialog()
	{
		if (m_ShowBuildDialog)
			ImGui::OpenPopup("Build Game");

		if (ImGui::BeginPopupModal("Build Game", &m_ShowBuildDialog, ImGuiWindowFlags_AlwaysAutoResize))
		{
			auto& settings = ProjectSettings::Get();

			ImGui::Text("Package the current project into a standalone, playable game.");
			ImGui::Spacing();

			// Build mode selector
			ImGui::Text("Build Mode");
			ImGui::SameLine();
			ImGui::PushItemWidth(180);
			const char* modes[] = { "Content Only (no rebuild)", "Full Build (MSBuild)" };
			ImGui::Combo("##BuildMode", &m_BuildMode, modes, IM_ARRAYSIZE(modes));
			ImGui::PopItemWidth();

			// Config selector
			ImGui::Text("Config");
			ImGui::SameLine();
			ImGui::PushItemWidth(180);
			const char* configs[] = { "Debug", "Release", "Dist" };
			ImGui::Combo("##BuildConfig", &m_BuildConfig, configs, IM_ARRAYSIZE(configs));
			ImGui::PopItemWidth();

			// Game project name (only relevant for Full Build)
			if (m_BuildMode == 1)
			{
				ImGui::Text("Game Project");
				ImGui::SameLine();
				ImGui::PushItemWidth(200);
				if (ImGui::InputText("##GameProject", &settings.GameProjectName))
				{
					if (settings.GameProjectName.empty())
						settings.GameProjectName = "CandyGame";
				}
				if (ImGui::IsItemDeactivatedAfterEdit())
					settings.Save();
				ImGui::PopItemWidth();
			}
			else
			{
				ImGui::Text("Content Only: uses existing CandyGame.exe, no C++ rebuild needed.");
			}

		ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Build", ImVec2(120, 0)))
			{
				m_ShowBuildDialog = false;
				if (m_BuildMode == 0)
					BuildGame_ContentOnly();
				else
					BuildGame_Full();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				m_ShowBuildDialog = false;
			}

			ImGui::EndPopup();
		}
	}

	void EditorLayer::BuildGame_Full()
	{
		auto& settings = ProjectSettings::Get();
		// Default to CandyGame so the current project builds out of the box
		if (settings.GameProjectName.empty())
			settings.GameProjectName = "CandyGame";

		auto project = Application::Get().GetProject();
		if (!project)
		{
			CANDY_CORE_ERROR("No project open. Open a project before building.");
			return;
		}

		// Resolve paths
		auto engineRoot = ProjectUtils::GetEnginePath().parent_path(); // e.g. "../../" from editor cwd
		const char* configNames[] = { "Debug", "Release", "Dist" };
		auto outputDir = std::string(configNames[m_BuildConfig]) + "-windows-x86_64";
		auto gameExeName = settings.GameProjectName + ".exe";
		auto binDir = engineRoot / "bin" / outputDir / settings.GameProjectName;
		auto binExe = binDir / gameExeName;

		// ---------- Step 1: Build the game project ----------
		auto msbuild = FindMSBuild();
		if (msbuild.empty())
		{
			CANDY_CORE_ERROR("Could not locate MSBuild.exe. Install the 'Desktop development with C++' workload in Visual Studio.");
			return;
		}
		{
			std::string cmd = "pushd \"";
			cmd += engineRoot.string();
			cmd += "\" && \"";
			cmd += msbuild;
			cmd += "\" CandyEngine.sln /t:";
			cmd += settings.GameProjectName;
			cmd += " /p:Configuration=";
			cmd += configNames[m_BuildConfig];
			cmd += " /nologo /v:minimal";
			CANDY_CORE_INFO("Running build: {0}", cmd);
			int rc = std::system(cmd.c_str());
			if (rc != 0)
			{
				CANDY_CORE_ERROR("Build failed (exit code {0}).", rc);
				return;
			}
		}

		if (!std::filesystem::exists(binExe))
		{
			CANDY_CORE_ERROR("Build output not found: {0}", binExe.string());
			return;
		}

		// ---------- Step 2: Assemble distribution folder ----------
		auto buildDir = project->GetProjectDirectory() / "Build" / configNames[m_BuildConfig];
		std::filesystem::create_directories(buildDir);

		// Copy game executable
		std::filesystem::copy_file(binExe, buildDir / gameExeName,
			std::filesystem::copy_options::overwrite_existing);

		// Copy all DLLs from the build output directory
		for (auto& entry : std::filesystem::directory_iterator(binDir))
		{
			if (entry.path().extension() == ".dll")
				std::filesystem::copy_file(entry.path(), buildDir / entry.path().filename(),
					std::filesystem::copy_options::overwrite_existing);
		}

		// ---------- Step 3: Engine content ----------
		// Place engine content at Build/Content/ so the standalone game finds it via VFS /engine
		auto engineContent = ProjectUtils::GetEngineContentPath();
		if (std::filesystem::exists(engineContent))
		{
			auto targetContent = buildDir / "Content";
			std::filesystem::copy(engineContent, targetContent,
				std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
			// Also produce a .pak for the VFS fast path
			PakFile::Pack(engineContent, buildDir / "Content.pak");
		}

		// ---------- Step 4: Project content ----------
		// Place the project file and content under Build/<ProjectName>/
		// The launch script runs: CandyGame.exe "<ProjectName>/<ProjectName>.candyproj"
		auto projectName = project->GetName();
		auto projectDestDir = buildDir / projectName;
		std::filesystem::create_directories(projectDestDir);

		// Copy the .candyproj file
		auto srcProj = project->GetProjectFileName();
		if (std::filesystem::exists(srcProj))
			std::filesystem::copy_file(srcProj, projectDestDir / srcProj.filename(),
				std::filesystem::copy_options::overwrite_existing);

		// Copy project content
		auto projectContent = project->GetProjectDirectory() / "Content";
		if (std::filesystem::exists(projectContent))
		{
			auto destContent = projectDestDir / "Content";
			std::filesystem::copy(projectContent, destContent,
				std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
			// Also produce a .pak for the VFS fast path
			PakFile::Pack(projectContent, projectDestDir / "Content.pak");
		}

		// ---------- Step 5: Launch script ----------
		{
			auto batPath = buildDir / "Launch.bat";
			std::ofstream batFile(batPath);
			batFile << "@echo off\r\n";
			batFile << "start \"\" \"" << gameExeName << "\" \""
				<< projectName << "/" << srcProj.filename().string() << "\"\r\n";
			batFile.close();
		}

		// ---------- Step 6: Report packaged contents ----------
		CANDY_CORE_INFO("Game packaged to: {0}", buildDir.string());
		CANDY_CORE_INFO("Packaged contents:");
		std::error_code ec;
		for (auto it = std::filesystem::recursive_directory_iterator(buildDir, ec);
			it != std::filesystem::recursive_directory_iterator() && !ec; ++it)
		{
			auto rel = std::filesystem::relative(it->path(), buildDir, ec);
			if (!ec)
				CANDY_CORE_INFO("  {0}", rel.string());
		}

		// Open the output folder in the system file explorer
		FileDialogs::OpenInShell(buildDir.string());
	}

	void EditorLayer::BuildGame_ContentOnly()
	{
		auto project = Application::Get().GetProject();
		if (!project)
		{
			CANDY_CORE_ERROR("No project open. Open a project before building.");
			return;
		}

		// Resolve paths
		auto& settings = ProjectSettings::Get();
		auto engineRoot = ProjectUtils::GetEnginePath().parent_path();
		const char* configNames[] = { "Debug", "Release", "Dist" };
		auto buildOutputDir = std::string(configNames[m_BuildConfig]) + "-windows-x86_64";
		auto binDir = engineRoot / "bin" / buildOutputDir / "CandyGame";
		auto binExe = binDir / "CandyGame.exe";

		if (!std::filesystem::exists(binExe))
		{
			CANDY_CORE_ERROR("CandyGame.exe not found at {0}. Build the {1} config first.", binExe.string(), configNames[m_BuildConfig]);
			return;
		}

		// ---------- Step 1: Create output directory ----------
		auto buildDir = project->GetProjectDirectory() / "Build" / configNames[m_BuildConfig];
		std::filesystem::create_directories(buildDir);

		// ---------- Step 2: Copy game executable (renamed to project name) ----------
		auto projectName = project->GetName();
		auto gameExeName = projectName + ".exe";
		std::filesystem::copy_file(binExe, buildDir / gameExeName,
			std::filesystem::copy_options::overwrite_existing);

		// ---------- Step 3: Copy all DLLs from the build output directory ----------
		for (auto& entry : std::filesystem::directory_iterator(binDir))
		{
			if (entry.path().extension() == ".dll")
				std::filesystem::copy_file(entry.path(), buildDir / entry.path().filename(),
					std::filesystem::copy_options::overwrite_existing);
		}

		// Also copy python314.zip and ._pth if they exist
		for (auto& entry : std::filesystem::directory_iterator(binDir))
		{
			auto ext = entry.path().extension();
			if (ext == ".zip" || ext == "._pth")
				std::filesystem::copy_file(entry.path(), buildDir / entry.path().filename(),
					std::filesystem::copy_options::overwrite_existing);
		}

		// ---------- Step 4: Create a single .pak with all content ----------
		// Structure inside the pak:
		//   engine/        <- engine Content/ files
		//   game/          <- game Content/ files (Config/, project.candyproj)
		auto engineContent = ProjectUtils::GetEngineContentPath();
		auto projectContent = project->GetProjectDirectory() / "Content";
		auto projFile = project->GetProjectFileName();

		// Use a temp staging directory to assemble the content
		auto stagingDir = buildDir / ".staging";
		std::filesystem::remove_all(stagingDir);
		std::filesystem::create_directories(stagingDir / "engine");
		std::filesystem::create_directories(stagingDir / "game");

		// Copy engine content into staging/engine/
		if (std::filesystem::exists(engineContent))
		{
			std::filesystem::copy(engineContent, stagingDir / "engine",
				std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
		}

		// Copy project content into staging/game/
		if (std::filesystem::exists(projectContent))
		{
			std::filesystem::copy(projectContent, stagingDir / "game",
				std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
		}

		// Copy project Config/ into staging/game/Config/
		auto projectConfig = project->GetProjectDirectory() / "Config";
		if (std::filesystem::exists(projectConfig))
		{
			std::filesystem::copy(projectConfig, stagingDir / "game" / "Config",
				std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
		}

		// Copy the .candyproj file into staging/game/ (so it's accessible as VFS://Game/project.candyproj)
		if (std::filesystem::exists(projFile))
		{
			std::filesystem::copy_file(projFile, stagingDir / "game" / "project.candyproj",
				std::filesystem::copy_options::overwrite_existing);
		}

		// Pack the staging directory into a single .pak
		auto pakPath = buildDir / (projectName + ".pak");
		CANDY_CORE_INFO("Packing content into {0}...", pakPath.string());
		if (!PakFile::Pack(stagingDir, pakPath))
		{
			CANDY_CORE_ERROR("Failed to create content pak.");
			std::filesystem::remove_all(stagingDir);
			return;
		}

		// Clean up staging
		std::filesystem::remove_all(stagingDir);

		// ---------- Step 5: Report ----------
		CANDY_CORE_INFO("Game packaged to: {0}", buildDir.string());
		CANDY_CORE_INFO("Packaged contents:");
		std::error_code ec;
		for (auto it = std::filesystem::recursive_directory_iterator(buildDir, ec);
			it != std::filesystem::recursive_directory_iterator() && !ec; ++it)
		{
			auto rel = std::filesystem::relative(it->path(), buildDir, ec);
			if (!ec)
				CANDY_CORE_INFO("  {0}", rel.string());
		}

		FileDialogs::OpenInShell(buildDir.string());
	}
}
