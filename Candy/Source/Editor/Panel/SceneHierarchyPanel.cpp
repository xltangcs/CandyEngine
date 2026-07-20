#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <glm/gtc/type_ptr.hpp>

#include "SceneHierarchyPanel.h"

#include "ImGuiUtils.h"

#include "Runtime/Scene/Components.h"
#include "Runtime/Utils/PlatformUtils.h"

#include <cstring>
#include <regex>
#include <fstream>

#include "Runtime/Core/Application.h"
#include "Runtime/Project/ProjectUtils.h"
#include "Runtime/Core/VfsPath.h"
#include "Runtime/Core/FileSystem.h"

/* The Microsoft C++ compiler is non-compliant with the C++ standard and needs
 * the following definition to disable a security warning on std::strncpy().
 */
#ifdef _MSVC_LANG
	#define _CRT_SECURE_NO_WARNINGS
#endif


namespace Candy {
	
	static std::string ParsePythonClassNameFromContent(const std::string& content)
	{
		std::regex pattern(R"(class\s+(\w+)\s*\([^)]*\bcandy\b\s*\.\s*ScriptObject\b[^)]*\))");
		std::smatch match;
		if (std::regex_search(content, match, pattern))
			return match[1];

		return {};
	}

	static std::string ParsePythonClassName(const std::filesystem::path& filePath)
	{
		std::filesystem::path absPath = std::filesystem::absolute(filePath);
		std::ifstream file(absPath);
		if (!file.is_open())
			return {};

		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		return ParsePythonClassNameFromContent(content);
	}

	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context)
	{
		SetContext(context);
	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& context)
	{
		m_Context = context;
		m_SelectionContext = {};
	}

	void SceneHierarchyPanel::OnImGuiRender()
	{
		ImGui::Begin("Scene Hierarchy");

		if (m_Context)
		{
			for (const auto [entityID] : m_Context->m_Registry.storage<entt::entity>().reach())
			{
				Entity entity{ entityID , m_Context.get() };
				if (!entity)
					return;
				DrawEntityNode(entity);
			}

			if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
				m_SelectionContext = {};

			// Right-click on blank space
			if (ImGui::BeginPopupContextWindow(0, 1 | ImGuiPopupFlags_NoOpenOverItems))
			{
				if (ImGui::MenuItem("Create Empty Entity"))
					m_Context->CreateEntity("Empty Entity");

				ImGui::EndPopup();
			}
		}

		ImGui::End();

		ImGui::Begin("Properties");
		if (m_SelectionContext)
		{
			DrawComponents(m_SelectionContext);
		}

		ImGui::InvisibleButton("##ScriptDropZone", ImGui::GetContentRegionAvail());
		
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				const char* path = (const char*)payload->Data;
				VfsPath vp = VfsPath::Parse(path);
				std::filesystem::path relPath(vp.relativePath);
				if (vp.IsValid() && relPath.extension() == ".py" && m_SelectionContext)
				{
					auto& sc = m_SelectionContext.HasComponent<ScriptComponent>()
						? m_SelectionContext.GetComponent<ScriptComponent>()
						: m_SelectionContext.AddComponent<ScriptComponent>();
					sc.ScriptPath = vp.ToString();
					auto content = FileSystem::Get().ReadText(vp.ToString());
					if (content)
					{
						std::string parsedName = ParsePythonClassNameFromContent(*content);
						if (!parsedName.empty())
							sc.ClassName = parsedName;
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::End();
	}

	void SceneHierarchyPanel::SetSelectedEntity(Entity entity)
	{
		m_SelectionContext = entity;
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		auto& tag = entity.GetComponent<TagComponent>().Tag;

		ImGuiTreeNodeFlags flags = ((m_SelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());
		if (ImGui::IsItemClicked())
		{
			m_SelectionContext = entity;
		}

		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Delete Entity"))
				entityDeleted = true;

			ImGui::EndPopup();
		}

		// Drag-drop target for .py script files
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				const char* path = (const char*)payload->Data;
				VfsPath vp = VfsPath::Parse(path);
				std::filesystem::path relPath(vp.relativePath);
				if (vp.IsValid() && relPath.extension() == ".py")
				{
					auto& sc = entity.HasComponent<ScriptComponent>()
						? entity.GetComponent<ScriptComponent>()
						: entity.AddComponent<ScriptComponent>();
					sc.ScriptPath = vp.ToString();
					auto content = FileSystem::Get().ReadText(vp.ToString());
					if (content)
					{
						std::string parsedName = ParsePythonClassNameFromContent(*content);
						if (!parsedName.empty())
							sc.ClassName = parsedName;
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (opened)
		{
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			bool opened = ImGui::TreeNodeEx((void*)9817239, flags, tag.c_str());
			if (opened)
				ImGui::TreePop();
			ImGui::TreePop();
		}

		if (entityDeleted)
		{
			m_Context->DestroyEntity(entity);
			if (m_SelectionContext == entity)
				m_SelectionContext = {};
		}

	}

	template<typename T, typename UIFunction>

	static void DrawComponent(const std::string& name, Entity entity, UIFunction uiFunction)
	{
		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding;
		if (entity.HasComponent<T>())
		{
			auto& component = entity.GetComponent<T>();
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			float lineHeight = ImGui::GetFrameHeight();
			ImGui::Separator();
			bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, name.c_str());
			ImGui::PopStyleVar(
			);
			ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
			if (ImGui::Button("+", ImVec2{ lineHeight, lineHeight }))
			{
				ImGui::OpenPopup("ComponentSettings");
			}

			bool removeComponent = false;
			if (ImGui::BeginPopup("ComponentSettings"))
			{
				if (ImGui::MenuItem("Remove component"))
					removeComponent = true;

				ImGui::EndPopup();
			}

			if (open)
			{
				uiFunction(component);
				ImGui::TreePop();
			}

			if (removeComponent)
				entity.RemoveComponent<T>();
		}
	}
	void SceneHierarchyPanel::DrawComponents(Entity entity)
	{
		if (entity.HasComponent<TagComponent>())
		{
			auto& tag = entity.GetComponent<TagComponent>().Tag;

			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			std::strncpy(buffer, tag.c_str(), sizeof(buffer));
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
			{
				tag = std::string(buffer);
			}
		}

		ImGui::SameLine();
		ImGui::PushItemWidth(-1);

		if (ImGui::Button("Add Component"))
		{
			ImGui::OpenPopup("AddComponent");
		}

		if (ImGui::BeginPopup("AddComponent"))
		{
			if (!m_SelectionContext.HasComponent<CameraComponent>())
			{
				if (ImGui::MenuItem("Camera"))
				{
					m_SelectionContext.AddComponent<CameraComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!m_SelectionContext.HasComponent<SpriteRendererComponent>())
			{
				if (ImGui::MenuItem("Sprite Renderer"))
				{
					m_SelectionContext.AddComponent<SpriteRendererComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!m_SelectionContext.HasComponent<CircleRendererComponent>())
			{
				if (ImGui::MenuItem("Circle Renderer"))
				{
					m_SelectionContext.AddComponent<CircleRendererComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!m_SelectionContext.HasComponent<Rigidbody2DComponent>())
			{
				if (ImGui::MenuItem("Rigidbody 2D"))
				{
					m_SelectionContext.AddComponent<Rigidbody2DComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!m_SelectionContext.HasComponent<BoxCollider2DComponent>())
			{
				if (ImGui::MenuItem("Box Collider 2D"))
				{
					m_SelectionContext.AddComponent<BoxCollider2DComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!m_SelectionContext.HasComponent<CircleCollider2DComponent>())
			{
				if (ImGui::MenuItem("Circle Collider 2D"))
				{
					m_SelectionContext.AddComponent<CircleCollider2DComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!m_SelectionContext.HasComponent<ScriptComponent>())
			{
				if (ImGui::MenuItem("Script"))
				{
					m_SelectionContext.AddComponent<ScriptComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!m_SelectionContext.HasComponent<AudioSourceComponent>())
			{
				if (ImGui::MenuItem("Audio Source"))
				{
					m_SelectionContext.AddComponent<AudioSourceComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!m_SelectionContext.HasComponent<UITextBlockComponent>())
			{
				if (ImGui::MenuItem("Text Block"))
				{
					m_SelectionContext.AddComponent<UITextBlockComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!m_SelectionContext.HasComponent<UIButtonComponent>())
			{
				if (ImGui::MenuItem("Button"))
				{
					m_SelectionContext.AddComponent<UIButtonComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}

		ImGui::PopItemWidth();

		DrawComponent<TransformComponent>("Transform", entity, [](auto& component)
			{
				ImGuiUtils::DrawVec3Control("Translation", component.Translation);
				glm::vec3 rotation = glm::degrees(component.Rotation);
				ImGuiUtils::DrawVec3Control("Rotation", rotation);
				component.Rotation = glm::radians(rotation);
				ImGuiUtils::DrawVec3Control("Scale", component.Scale, 1.0f);
			});

		DrawComponent<CameraComponent>("Camera", entity, [](auto& component)
			{
				auto& camera = component.Camera;

				const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
				int projType = (int)camera.GetProjectionType();
				if (ImGuiUtils::DrawCombo("Projection", projectionTypeStrings, 2, projType))
					camera.SetProjectionType((SceneCamera::ProjectionType)projType);

				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
				{
					float fov = glm::degrees(camera.GetPerspectiveVerticalFOV());
					if (ImGuiUtils::DrawDragFloat("Vertical FOV", fov))
						camera.SetPerspectiveVerticalFOV(glm::radians(fov));

					float clipNear = camera.GetPerspectiveNearClip();
					if (ImGuiUtils::DrawDragFloat("Near", clipNear))
						camera.SetPerspectiveNearClip(clipNear);

					float clipFar = camera.GetPerspectiveFarClip();
					if (ImGuiUtils::DrawDragFloat("Far", clipFar))
						camera.SetPerspectiveFarClip(clipFar);
				}

				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
				{
					float size = camera.GetOrthographicSize();
					if (ImGuiUtils::DrawDragFloat("Size", size))
						camera.SetOrthographicSize(size);

					float orthoNear = camera.GetOrthographicNearClip();
					if (ImGuiUtils::DrawDragFloat("Near", orthoNear))
						camera.SetOrthographicNearClip(orthoNear);

					float orthoFar = camera.GetOrthographicFarClip();
					if (ImGuiUtils::DrawDragFloat("Far", orthoFar))
						camera.SetOrthographicFarClip(orthoFar);

					ImGuiUtils::DrawCheckbox("Fixed Aspect Ratio", component.FixedAspectRatio);
				}
			});

		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [](auto& component)
			{
				ImGuiUtils::DrawColorEdit4("Color", component.Color);

				if (ImGuiUtils::DrawContentPathControl("Texture", component.TexturePath))
				{
					Ref<Texture2D> tex = Texture2D::Create(component.TexturePath);
					if (tex && tex->IsLoaded())
					{
						component.Texture = tex;
					}
					else
						CANDY_WARN("Could not load texture {0}", component.TexturePath);
				}

				ImGuiUtils::DrawDragFloat("Tiling Factor", component.TilingFactor, 0.1f, 0.0f, 100.0f);
			});

		DrawComponent<CircleRendererComponent>("Circle Renderer", entity, [](auto& component)
			{
				ImGuiUtils::DrawColorEdit4("Color", component.Color);
				ImGuiUtils::DrawDragFloat("Thickness", component.Thickness, 0.025f, 0.0f, 1.0f);
				ImGuiUtils::DrawDragFloat("Fade", component.Fade, 0.00025f, 0.0f, 1.0f);
			});

		DrawComponent<Rigidbody2DComponent>("Rigidbody 2D", entity, [](auto& component)
			{
				const char* bodyTypeStrings[] = { "Static", "Dynamic", "Kinematic" };
				int bodyType = (int)component.Type;
				if (ImGuiUtils::DrawCombo("Body Type", bodyTypeStrings, 3, bodyType))
					component.Type = (Rigidbody2DComponent::BodyType)bodyType;

				ImGuiUtils::DrawCheckbox("Fixed Rotation", component.FixedRotation);
			});

		DrawComponent<BoxCollider2DComponent>("Box Collider 2D", entity, [](auto& component)
			{
				ImGuiUtils::DrawDragFloat2("Offset", component.Offset);
				ImGuiUtils::DrawDragFloat2("Size", component.Size);
				ImGuiUtils::DrawDragFloat("Density", component.Density, 0.01f, 0.0f, 1.0f);
				ImGuiUtils::DrawDragFloat("Friction", component.Friction, 0.01f, 0.0f, 1.0f);
				ImGuiUtils::DrawDragFloat("Restitution", component.Restitution, 0.01f, 0.0f, 1.0f);
				ImGuiUtils::DrawDragFloat("Restitution Threshold", component.RestitutionThreshold, 0.01f, 0.0f);
			});

		DrawComponent<CircleCollider2DComponent>("Circle Collider 2D", entity, [](auto& component)
			{
				ImGuiUtils::DrawDragFloat2("Offset", component.Offset);
				ImGuiUtils::DrawDragFloat("Radius", component.Radius);
				ImGuiUtils::DrawDragFloat("Density", component.Density, 0.01f, 0.0f, 1.0f);
				ImGuiUtils::DrawDragFloat("Friction", component.Friction, 0.01f, 0.0f, 1.0f);
				ImGuiUtils::DrawDragFloat("Restitution", component.Restitution, 0.01f, 0.0f, 1.0f);
				ImGuiUtils::DrawDragFloat("Restitution Threshold", component.RestitutionThreshold, 0.01f, 0.0f);
			});

		DrawComponent<ScriptComponent>("Script", entity, [](auto& component)
		{
			if (ImGuiUtils::DrawContentPathControl("Script Path", component.ScriptPath))
			{
				auto content = FileSystem::Get().ReadText(component.ScriptPath);
				if (content)
				{
					std::string parsedName = ParsePythonClassNameFromContent(*content);
					if (!parsedName.empty())
						component.ClassName = parsedName;
				}
			}

			ImGuiUtils::DrawInputText("Class Name", component.ClassName);
		});

		DrawComponent<AudioSourceComponent>("Audio Source", entity, [](auto& component)
		{
			ImGuiUtils::DrawContentPathControl("Sound Path", component.SoundPath);
			ImGuiUtils::DrawDragFloat("Volume", component.Volume, 0.01f, 0.0f, 1.0f);
			ImGuiUtils::DrawCheckbox("Looping", component.Looping);
			ImGuiUtils::DrawCheckbox("Play On Start", component.PlayOnStart);
		});

		DrawComponent<UITextBlockComponent>("Text Blocks", entity, [](auto& component)
		{
			std::string toRemove;
			float lineHeight = ImGui::GetFrameHeight();

			for (auto& [key, tb] : component.TextBlocks)
			{
				ImGui::PushID(key.c_str());
				bool open = ImGui::CollapsingHeader(key.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
				ImGui::SameLine(ImGui::GetContentRegionAvail().x - lineHeight);
				if (ImGui::Button("X", ImVec2{ lineHeight, lineHeight }))
					toRemove = key;
				if (open)
				{
					ImGui::Indent();

					ImGuiUtils::DrawInputText("Text", tb.Text);
					ImGuiUtils::DrawColorEdit4("Color", tb.Color);
					ImGuiUtils::DrawDragFloat2("Position", tb.Position);
					ImGuiUtils::DrawDragFloat("Font Size", tb.FontSize, 1.0f, 1.0f, 200.0f);
					ImGuiUtils::DrawCheckbox("Visible", tb.Visible);

					ImGui::Unindent();
				}
				ImGui::PopID();
			}
			if (!toRemove.empty())
				component.TextBlocks.erase(toRemove);

			ImGui::Separator();
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x * 0.5f - 60.0f);
			if (ImGui::Button("+ TextBlock", ImVec2{ 120.0f, 0.0f }))
			{
				static uint32_t s_TextBlockCounter = 0;
				std::string key = "TextBlock_" + std::to_string(++s_TextBlockCounter);
				component.TextBlocks[key] = TextBlockUIData();
			}
		});

		DrawComponent<UIButtonComponent>("Buttons", entity, [](auto& component)
		{
			std::string toRemove;
			float lineHeight = ImGui::GetFrameHeight();

			for (auto& [key, btn] : component.Buttons)
			{
				ImGui::PushID(key.c_str());
				bool open = ImGui::CollapsingHeader(key.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
				ImGui::SameLine(ImGui::GetContentRegionAvail().x - lineHeight);
				if (ImGui::Button("X", ImVec2{ lineHeight, lineHeight }))
					toRemove = key;
				if (open)
				{
					ImGui::Indent();

					ImGuiUtils::DrawInputText("Text", btn.Text);
					ImGuiUtils::DrawDragFloat2("Size", btn.Size);
					ImGuiUtils::DrawDragFloat2("Position", btn.Position);
					ImGuiUtils::DrawInputText("OnClick", btn.OnClick);
					ImGuiUtils::DrawCheckbox("Visible", btn.Visible);

					ImGui::Unindent();
				}
				ImGui::PopID();
			}
			if (!toRemove.empty())
				component.Buttons.erase(toRemove);

			ImGui::Separator();
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x * 0.5f - 60.0f);
			if (ImGui::Button("+ Button", ImVec2{ 120.0f, 0.0f }))
			{
				static uint32_t s_ButtonCounter = 0;
				std::string key = "Button_" + std::to_string(++s_ButtonCounter);
				component.Buttons[key] = ButtonUIData();
			}
		});
	}

}