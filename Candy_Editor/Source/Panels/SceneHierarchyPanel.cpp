#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

#include "SceneHierarchyPanel.h"

#include "Candy/Scene/Components.h"
#include "Candy/Utils/PlatformUtils.h"

#include <cstring>
#include <regex>
#include <fstream>

#include "Candy/Core/Application.h"
#include "Candy/Project/ProjectUtils.h"

/* The Microsoft C++ compiler is non-compliant with the C++ standard and needs
 * the following definition to disable a security warning on std::strncpy().
 */
#ifdef _MSVC_LANG
	#define _CRT_SECURE_NO_WARNINGS
#endif


namespace Candy {
	
	static std::string ParsePythonClassName(const std::filesystem::path& filePath)
	{
		std::filesystem::path absPath = std::filesystem::absolute(filePath);
		std::ifstream file(absPath);
		if (!file.is_open())
			return {};

		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		std::regex pattern(R"(class\s+(\w+)\s*\([^)]*\bcandy\b\s*\.\s*ScriptObject\b[^)]*\))");
		std::smatch match;
		if (std::regex_search(content, match, pattern))
			return match[1];

		return {};
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
				const wchar_t* path = (const wchar_t*)payload->Data;
				std::filesystem::path relPath(path);
				if (relPath.extension() == ".py" && m_SelectionContext)
				{
					auto& sc = m_SelectionContext.HasComponent<ScriptComponent>()
						? m_SelectionContext.GetComponent<ScriptComponent>()
						: m_SelectionContext.AddComponent<ScriptComponent>();
					sc.ScriptPath = relPath.generic_string();
					std::filesystem::path fullPath = std::filesystem::absolute(ProjectUtils::GetProjectContentPath() / relPath);
					std::string parsedName = ParsePythonClassName(fullPath);
					if (!parsedName.empty())
						sc.ClassName = parsedName;
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
				const wchar_t* path = (const wchar_t*)payload->Data;
				std::filesystem::path relPath(path);
				if (relPath.extension() == ".py")
				{
					auto& sc = entity.HasComponent<ScriptComponent>()
						? entity.GetComponent<ScriptComponent>()
						: entity.AddComponent<ScriptComponent>();
					sc.ScriptPath = relPath.generic_string();
					std::filesystem::path fullPath = std::filesystem::absolute(ProjectUtils::GetProjectContentPath() / relPath);
					std::string parsedName = ParsePythonClassName(fullPath);
					if (!parsedName.empty())
						sc.ClassName = parsedName;
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

	static void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f)
	{
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float lineHeight = ImGui::GetFrameHeight();
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		if (ImGui::Button("X", buttonSize))
			values.x = resetValue;
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		if (ImGui::Button("Y", buttonSize))
			values.y = resetValue;
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		if (ImGui::Button("Z", buttonSize))
			values.z = resetValue;
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();
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
				DrawVec3Control("Translation", component.Translation);
				glm::vec3 rotation = glm::degrees(component.Rotation);
				DrawVec3Control("Rotation", rotation);
				component.Rotation = glm::radians(rotation);
				DrawVec3Control("Scale", component.Scale, 1.0f);
			});

		DrawComponent<CameraComponent>("Camera", entity, [](auto& component)
			{
				auto& camera = component.Camera;

				const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
				const char* currentProjectionTypeString = projectionTypeStrings[(int)camera.GetProjectionType()];
				if (ImGui::BeginCombo("Projection", currentProjectionTypeString))
				{
					for (int i = 0; i < 2; i++)
					{
						bool isSelected = currentProjectionTypeString == projectionTypeStrings[i];
						if (ImGui::Selectable(projectionTypeStrings[i], isSelected))
						{
							currentProjectionTypeString = projectionTypeStrings[i];
							camera.SetProjectionType((SceneCamera::ProjectionType)i);
						}

						if (isSelected)
							ImGui::SetItemDefaultFocus();
					}

					ImGui::EndCombo();
				}

				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
				{
					float perspectiveVerticalFov = glm::degrees(camera.GetPerspectiveVerticalFOV());
					if (ImGui::DragFloat("Vertical FOV", &perspectiveVerticalFov))
						camera.SetPerspectiveVerticalFOV(glm::radians(perspectiveVerticalFov));

					float perspectiveNear = camera.GetPerspectiveNearClip();
					if (ImGui::DragFloat("Near", &perspectiveNear))
						camera.SetPerspectiveNearClip(perspectiveNear);

					float perspectiveFar = camera.GetPerspectiveFarClip();
					if (ImGui::DragFloat("Far", &perspectiveFar))
						camera.SetPerspectiveFarClip(perspectiveFar);
				}

				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
				{
					float orthoSize = camera.GetOrthographicSize();
					if (ImGui::DragFloat("Size", &orthoSize))
						camera.SetOrthographicSize(orthoSize);

					float orthoNear = camera.GetOrthographicNearClip();
					if (ImGui::DragFloat("Near", &orthoNear))
						camera.SetOrthographicNearClip(orthoNear);

					float orthoFar = camera.GetOrthographicFarClip();
					if (ImGui::DragFloat("Far", &orthoFar))
						camera.SetOrthographicFarClip(orthoFar);

					ImGui::Checkbox("Fixed Aspect Ratio", &component.FixedAspectRatio);
				}
			});

		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [](auto& component)
			{
				ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));

				ImGui::Button("Texture", ImVec2(100.0f, 0.0f));
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
					{
						const wchar_t* path = (const wchar_t*)payload->Data;
						std::filesystem::path texturePath = std::filesystem::path(ProjectUtils::GetProjectContentPath()) / path;
						Ref<Texture2D> texture = Texture2D::Create(texturePath.string());
						if (texture->IsLoaded())
							component.Texture = texture;
						else
							CANDY_WARN("Could not load texture {0}", texturePath.filename().string());
					}
					ImGui::EndDragDropTarget();
				}

				ImGui::DragFloat("Tiling Factor", &component.TilingFactor, 0.1f, 0.0f, 100.0f);
			});

		DrawComponent<CircleRendererComponent>("Circle Renderer", entity, [](auto& component)
			{
				ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));
				ImGui::DragFloat("Thickness", &component.Thickness, 0.025f, 0.0f, 1.0f);
				ImGui::DragFloat("Fade", &component.Fade, 0.00025f, 0.0f, 1.0f);
			});

		DrawComponent<Rigidbody2DComponent>("Rigidbody 2D", entity, [](auto& component)
			{
				const char* bodyTypeStrings[] = { "Static", "Dynamic", "Kinematic" };
				const char* currentBodyTypeString = bodyTypeStrings[(int)component.Type];
				if (ImGui::BeginCombo("Body Type", currentBodyTypeString))
				{
					for (int i = 0; i < 2; i++)
					{
						bool isSelected = currentBodyTypeString == bodyTypeStrings[i];
						if (ImGui::Selectable(bodyTypeStrings[i], isSelected))
						{
							currentBodyTypeString = bodyTypeStrings[i];
							component.Type = (Rigidbody2DComponent::BodyType)i;
						}

						if (isSelected)
							ImGui::SetItemDefaultFocus();
					}

					ImGui::EndCombo();
				}

				ImGui::Checkbox("Fixed Rotation", &component.FixedRotation);
			});

		DrawComponent<BoxCollider2DComponent>("Box Collider 2D", entity, [](auto& component)
			{
				ImGui::DragFloat2("Offset", glm::value_ptr(component.Offset));
				ImGui::DragFloat2("Size", glm::value_ptr(component.Size));
				ImGui::DragFloat("Density", &component.Density, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Restitution Threshold", &component.RestitutionThreshold, 0.01f, 0.0f);
			});

		DrawComponent<CircleCollider2DComponent>("Circle Collider 2D", entity, [](auto& component)
			{
				ImGui::DragFloat2("Offset", glm::value_ptr(component.Offset));
				ImGui::DragFloat("Radius", &component.Radius);
				ImGui::DragFloat("Density", &component.Density, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Restitution Threshold", &component.RestitutionThreshold, 0.01f, 0.0f);
			});

		DrawComponent<ScriptComponent>("Script", entity, [](auto& component)
		{
			char buffer[256];

			// ScriptPath
			memset(buffer, 0, sizeof(buffer));
			std::strncpy(buffer, component.ScriptPath.c_str(), sizeof(buffer));
			ImGui::Text("ScriptPath");
			ImGui::SameLine();
			ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
			if (ImGui::InputText("##ScriptPath", buffer, sizeof(buffer)))
				component.ScriptPath = buffer;
			ImGui::PopItemWidth();

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					const wchar_t* path = (const wchar_t*)payload->Data;
					std::filesystem::path relPath(path);
					if (relPath.extension() == ".py")
					{
						component.ScriptPath = relPath.generic_string();
						std::strncpy(buffer, component.ScriptPath.c_str(), sizeof(buffer) - 1);
						buffer[sizeof(buffer) - 1] = '\0';

						std::filesystem::path fullPath = std::filesystem::absolute(ProjectUtils::GetProjectContentPath() / relPath);
						std::string parsedName = ParsePythonClassName(fullPath);
						if (!parsedName.empty())
							component.ClassName = parsedName;
					}
				}
				ImGui::EndDragDropTarget();
			}
			
			ImGui::SameLine();
			if (ImGui::Button("..."))
			{
				std::string filepath = FileDialogs::OpenFile("Python Script (*.py)\0*.py\0");
				if (!filepath.empty())
				{
					std::filesystem::path relPath = std::filesystem::relative(std::filesystem::path(filepath), ProjectUtils::GetProjectContentPath());
					component.ScriptPath = relPath.generic_string();
					std::strncpy(buffer, component.ScriptPath.c_str(), sizeof(buffer) - 1);
					buffer[sizeof(buffer) - 1] = '\0';

					std::string parsedName = ParsePythonClassName(filepath);
					if (!parsedName.empty())
						component.ClassName = parsedName;
				}
			}
			
			// ClassName
			memset(buffer, 0, sizeof(buffer));
			std::strncpy(buffer, component.ClassName.c_str(), sizeof(buffer));
			ImGui::Text("ClassName");
			ImGui::SameLine();
			if (ImGui::InputText("##ClassName", buffer, sizeof(buffer)))
				component.ClassName = buffer;
		});

		DrawComponent<AudioSourceComponent>("Audio Source", entity, [](auto& component)
		{
			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			std::strncpy(buffer, component.SoundPath.c_str(), sizeof(buffer));
			ImGui::Text("Sound Path");
			ImGui::SameLine();
			if (ImGui::InputText("##SoundPath", buffer, sizeof(buffer)))
			{
				component.SoundPath = buffer;
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					const wchar_t* path = (const wchar_t*)payload->Data;
					std::filesystem::path fullPath = std::filesystem::path(ProjectUtils::GetProjectContentPath()) / path;
					component.SoundPath = fullPath.string();
					std::strncpy(buffer, component.SoundPath.c_str(), sizeof(buffer) - 1);
					buffer[sizeof(buffer) - 1] = '\0';
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::DragFloat("Volume", &component.Volume, 0.01f, 0.0f, 1.0f);
			ImGui::Checkbox("Looping", &component.Looping);
			ImGui::Checkbox("Play On Start", &component.PlayOnStart);
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

					char buf[256];
					memset(buf, 0, sizeof(buf));
					std::strncpy(buf, tb.Text.c_str(), sizeof(buf));
					if (ImGui::InputText("Text", buf, sizeof(buf)))
						tb.Text = buf;

					ImGui::ColorEdit4("Color", glm::value_ptr(tb.Color));
					ImGui::DragFloat2("Position", glm::value_ptr(tb.Position));
					ImGui::DragFloat("Font Size", &tb.FontSize, 1.0f, 1.0f, 200.0f);
					ImGui::Checkbox("Visible", &tb.Visible);

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

					char buf[256];
					memset(buf, 0, sizeof(buf));
					std::strncpy(buf, btn.Text.c_str(), sizeof(buf));
					if (ImGui::InputText("Text", buf, sizeof(buf)))
						btn.Text = buf;

					ImGui::DragFloat2("Size", glm::value_ptr(btn.Size));
					ImGui::DragFloat2("Position", glm::value_ptr(btn.Position));

					char funcBuf[256];
					memset(funcBuf, 0, sizeof(funcBuf));
					std::strncpy(funcBuf, btn.OnClick.c_str(), sizeof(funcBuf));
					if (ImGui::InputText("OnClick", funcBuf, sizeof(funcBuf)))
						btn.OnClick = funcBuf;

					ImGui::Checkbox("Visible", &btn.Visible);

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