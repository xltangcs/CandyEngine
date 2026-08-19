#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <glm/gtc/type_ptr.hpp>

#include "SceneHierarchyPanel.h"

#include "ImGuiUtils.h"

#include "ComponentUI.generated.inl"

#include "Runtime/Scene/Components.h"
#include "Runtime/Asset/MeshImporter.h"
#include "Runtime/Asset/Material.h"
#include "Runtime/Asset/MaterialCache.h"
#include "Runtime/Asset/ShaderCache.h"
#include "Runtime/Utils/PlatformUtils.h"
#include "EditorSelection.h"

#include <cstring>
#include <regex>
#include <fstream>
#include <algorithm>

#include "Runtime/Core/Application.h"
#include "Runtime/Core/VfsPath.h"
#include "Runtime/Core/FileSystem.h"

/* The Microsoft C++ compiler is non-compliant with the C++ standard and needs
 * the following definition to disable a security warning on std::strncpy().
 */
#ifdef _MSVC_LANG
	#define _CRT_SECURE_NO_WARNINGS
#endif


namespace Candy {
	
namespace
{
	template<typename T, typename UIFunction>
	void DrawComponent(const std::string& name, Entity entity, UIFunction uiFunction)
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
	std::string ParsePythonClassNameFromContent(const std::string& content)
	{
		std::regex pattern(R"(class\s+(\w+)\s*\([^)]*\bcandy\b\s*\.\s*ScriptObject\b[^)]*\))");
		std::smatch match;
		if (std::regex_search(content, match, pattern))
			return match[1];

		return {};
	}
	std::string ParsePythonClassName(const std::filesystem::path& filePath)
	{
		std::filesystem::path absPath = std::filesystem::absolute(filePath);
		std::ifstream file(absPath);
		if (!file.is_open())
			return {};

		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		return ParsePythonClassNameFromContent(content);
	}

	// Renders the inspector for the currently-selected asset (VFS path).
	// Supports material (.mat) and shader (.hlsl/.glsl) for now.

	// Draws an editable control for each reflected shader parameter. Changes
	// are written straight into the material's override table, mirroring how
	// Godot's inspector edits shader uniforms on a ShaderMaterial.
	void DrawShaderParameterControls(const Ref<Material>& mat, std::vector<ShaderParameter>& params, bool& dirty)
	{
		for (auto& param : params)
		{
			const std::string label = param.DisplayName.empty() ? param.Name : param.DisplayName;

			ImGui::PushID(param.Name.c_str());

			if (param.IsEnum)
			{
				std::vector<const char*> labels;
				labels.reserve(param.EnumLabels.size());
				for (const auto& l : param.EnumLabels)
					labels.push_back(l.c_str());

				int index = 0;
				if (std::holds_alternative<int>(param.Default))
					index = std::get<int>(param.Default);
				else if (std::holds_alternative<float>(param.Default))
					index = static_cast<int>(std::get<float>(param.Default));

				if (ImGuiUtils::DrawCombo(label, labels.data(), static_cast<int>(labels.size()), index))
				{
					param.Default = (param.Type == ShaderParamType::Int)
						? ShaderParamValue(index)
						: ShaderParamValue(static_cast<float>(index));
					mat->ShaderParams[param.Name] = param.Default;
					dirty = true;
				}
			}
			else
			{
				switch (param.Type)
				{
					case ShaderParamType::Float:
					{
						float value = std::get<float>(param.Default);
						bool modified = param.HasRange
							? ImGuiUtils::DrawSliderFloat(label, value, param.Range.Min, param.Range.Max)
							: ImGuiUtils::DrawDragFloat(label, value, param.Range.Step);
						if (modified)
						{
							param.Default = value;
							mat->ShaderParams[param.Name] = value;
							dirty = true;
						}
						break;
					}
					case ShaderParamType::Int:
					{
						int value = std::get<int>(param.Default);
						if (ImGuiUtils::DrawInputInt(label, value))
						{
							param.Default = value;
							mat->ShaderParams[param.Name] = value;
							dirty = true;
						}
						break;
					}
					case ShaderParamType::Bool:
					{
						bool value = std::get<bool>(param.Default);
						if (ImGuiUtils::DrawCheckbox(label, value))
						{
							param.Default = value;
							mat->ShaderParams[param.Name] = value;
							dirty = true;
						}
						break;
					}
					case ShaderParamType::Vec2:
					{
						glm::vec2 value = std::get<glm::vec2>(param.Default);
						if (ImGuiUtils::DrawDragFloat2(label, value))
						{
							param.Default = value;
							mat->ShaderParams[param.Name] = value;
							dirty = true;
						}
						break;
					}
					case ShaderParamType::Vec3:
					{
						glm::vec3 value = std::get<glm::vec3>(param.Default);
						const glm::vec3 before = value;
						ImGuiUtils::DrawVec3Control(label, value);
						if (value != before)
						{
							param.Default = value;
							mat->ShaderParams[param.Name] = value;
							dirty = true;
						}
						break;
					}
					case ShaderParamType::Vec4:
					{
						glm::vec4 value = std::get<glm::vec4>(param.Default);
						if (ImGuiUtils::DrawDragFloat4(label, value))
						{
							param.Default = value;
							mat->ShaderParams[param.Name] = value;
							dirty = true;
						}
						break;
					}
					case ShaderParamType::Color:
					{
						glm::vec4 value = std::get<glm::vec4>(param.Default);
						if (ImGuiUtils::DrawColorEdit4(label, value))
						{
							param.Default = value;
							mat->ShaderParams[param.Name] = value;
							dirty = true;
						}
						break;
					}
					case ShaderParamType::Texture:
					{
						std::string path = std::get<std::string>(param.Default);
						if (ImGuiUtils::DrawPathInput(label, path))
						{
							param.Default = path;
							mat->ShaderParams[param.Name] = path;
							dirty = true;
						}
						break;
					}
					default:
						break;
				}
			}

			ImGui::PopID();
		}
	}

	// Shows the full shader source (double-click a .hlsl/.glsl in the Content
	// Browser to open it). Read-only.
	void DrawShaderInspector(const std::string& vfsPath)
	{
		ImGui::TextDisabled("Shader Source");
		ImGui::Separator();
		ImGui::TextWrapped("%s", vfsPath.c_str());

		std::optional<std::string> text = FileSystem::Get().ReadText(vfsPath);
		if (!text)
		{
			ImGui::TextDisabled("Unable to read shader file.");
			return;
		}

		ImGui::Separator();
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		ImGui::InputTextMultiline("##source", &*text, avail, ImGuiInputTextFlags_ReadOnly);
	}

	void DrawMaterialInspector(const std::string& vfsPath)
	{
		Ref<Material> mat = MaterialCache::Get().Load(vfsPath);
		if (!mat)
		{
			ImGui::TextDisabled("Failed to load material");
			ImGui::TextWrapped("%s", vfsPath.c_str());
			return;
		}

		// Any modification is buffered then written back in one go per frame
		// so Material::Serialize is not called on every drag-step.
		bool dirty = false;

		// Identity
		dirty |= ImGuiUtils::DrawInputText("Name", mat->Name);

		ImGui::Separator();

		// Shader binding --- all editable parameters come from the shader's
		// //@param reflection (Godot-style). No shader = no parameters.
		if (ImGuiUtils::DrawPathInput("Shader", mat->ShaderPath,
			[] { ImGui::Text("Drop a .hlsl / .glsl shader to define the editable parameters."); }))
		{
			dirty = true;
		}

		std::vector<ShaderParameter> shaderParams;
		if (mat->GetShaderParameters(shaderParams))
		{
			ImGui::Separator();
			ImGui::TextDisabled("Shader Parameters");
			ImGui::Separator();
			DrawShaderParameterControls(mat, shaderParams, dirty);
		}

		ImGui::Separator();

		// Info / actions
		ImGui::TextDisabled("Path: %s", vfsPath.c_str());

		if (ImGui::Button("Save"))
		{
			if (mat->Serialize(vfsPath))
				MaterialCache::Get().Touch(vfsPath);
			dirty = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Reload"))
		{
			// Reload must drop the cached instances so the next Load re-reads
			// both the .mat and the bound shader from disk.
			MaterialCache::Get().Reload(vfsPath);
			ShaderCache::Get().Invalidate(mat->ShaderPath);
			mat = MaterialCache::Get().Load(vfsPath);
			dirty = false;
		}

		// Auto-save on any modification.
		if (dirty)
		{
			if (mat->Serialize(vfsPath))
				MaterialCache::Get().Touch(vfsPath);
			else
				CANDY_CORE_ERROR("Failed to serialize material {}", vfsPath);
		}
	}

}

	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context)
	{
		SetContext(context);
	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& context)
	{
		m_Context = context;
		m_SelectionContext = {};
		EditorSelection::Get().Clear();
	}

	void SceneHierarchyPanel::OnImGuiRender()
	{
		ImGui::Begin("Scene Hierarchy");

		if (m_Context)
		{
			std::vector<Entity> entitiesToDelete;
			for (const auto [entityID] : m_Context->m_Registry.storage<entt::entity>().reach())
			{
				Entity entity{ entityID , m_Context.get() };
				if (!entity)
					continue;
				if (DrawEntityNode(entity))
				{
					entitiesToDelete.push_back(entity);
				}
			}
			for (Entity& entity : entitiesToDelete)
				m_Context->DestroyEntity(entity);

			if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
			{
				m_SelectionContext = {};
				EditorSelection::Get().Clear();
			}

			// Right-click on blank space
			if (ImGui::BeginPopupContextWindow("##HierarchyBlankContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
			{
				if (ImGui::MenuItem("Create Empty Entity"))
					m_Context->CreateEntity("Empty Entity");

				ImGui::EndPopup();
			}
		}

		ImGui::End();

		ImGui::Begin("Properties");
		auto& selection = EditorSelection::Get();
		if (selection.HasAssetSelection())
		{
			DrawSelectedAsset(selection.GetSelectedAsset());
		}
		else if (m_SelectionContext)
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
		// Clicking an entity in the hierarchy clears any asset selection so the
		// Properties panel flips back to component editing.
		EditorSelection::Get().SelectEntity(entity);
	}

	void SceneHierarchyPanel::DrawSelectedAsset(const std::string& vfsPath)
	{
		if (vfsPath.empty())
		{
			ImGui::TextDisabled("No asset selected");
			return;
		}

		ImGui::TextDisabled("Asset Inspector");
		ImGui::Separator();

		if (EditorSelection::IsMaterial(vfsPath))
		{
			DrawMaterialInspector(vfsPath);
		}
		else if (EditorSelection::IsShader(vfsPath))
		{
			DrawShaderInspector(vfsPath);
		}
		else
		{
			ImGui::Text("No inspector available for this asset type.");
			ImGui::TextWrapped("%s", vfsPath.c_str());
		}
	}

	bool SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		auto& tag = entity.GetComponent<TagComponent>().Tag;

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen;

		if (m_SelectionContext == entity)
			flags |= ImGuiTreeNodeFlags_Selected;

		ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());
		if (ImGui::IsItemClicked())
		{
			m_SelectionContext = entity;
			EditorSelection::Get().SelectEntity(entity);
		}

		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && m_OnEntityDoubleClicked)
			m_OnEntityDoubleClicked(entity);

		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Delete Entity"))
				entityDeleted = true;

			ImGui::EndPopup();
		}

		if (entityDeleted)
		{
			if (m_SelectionContext == entity)
			{
				m_SelectionContext = {};
				EditorSelection::Get().Clear();
			}
			return true;
		}
		return false;
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

			if (!m_SelectionContext.HasComponent<LightComponent>())
			{
				if (ImGui::MenuItem("Light"))
				{
					m_SelectionContext.AddComponent<LightComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!m_SelectionContext.HasComponent<SkyboxComponent>())
			{
				if (ImGui::MenuItem("Skybox"))
				{
					m_SelectionContext.AddComponent<SkyboxComponent>();
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

			if (!m_SelectionContext.HasComponent<StaticMeshComponent>())
			{
				if (ImGui::MenuItem("Static Mesh"))
				{
					m_SelectionContext.AddComponent<StaticMeshComponent>();
					ImGui::CloseCurrentPopup();
				}
			}

			if (!m_SelectionContext.HasComponent<SkeletalMeshComponent>())
			{
				if (ImGui::MenuItem("Skeletal Mesh"))
				{
					m_SelectionContext.AddComponent<SkeletalMeshComponent>();
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

				if (ImGuiUtils::DrawPathInput("Texture", component.TexturePath))
				{
					if (component.TexturePath.empty())
						component.Texture.reset();
					else
					{
						Ref<Texture2D> tex = Texture2D::Create(component.TexturePath); // sprite maps are color maps -> default sRGB
						if (tex && tex->IsLoaded())
							component.Texture = tex;
						else
						{
							component.Texture.reset();
							CANDY_WARN("Could not load texture {0}", component.TexturePath);
						}
					}
				}

				ImGuiUtils::DrawDragFloat("Tiling Factor", component.TilingFactor, 0.1f, 0.0f, 100.0f);
			});

		DrawComponent<SkyboxComponent>("Skybox", entity, [](auto& component)
			{
				if (ImGuiUtils::DrawPathInput("Cubemap (equirect)", component.CubemapPath))
				{
					if (component.CubemapPath.empty())
						component.Cubemap.reset();
					else
					{
						Ref<TextureCubemap> cubemap = TextureCubemap::CreateFromEquirect(component.CubemapPath);
						if (cubemap)
							component.Cubemap = cubemap;
						else
						{
							component.Cubemap.reset();
							CANDY_WARN("Could not load cubemap {0}", component.CubemapPath);
						}
					}
				}

				ImGuiUtils::DrawDragFloat("Intensity (IBL)", component.Intensity, 0.05f, 0.0f, 10.0f);
				ImGuiUtils::DrawDragFloat("Exposure", component.Exposure, 0.05f, 0.0f, 10.0f);

				if (component.Cubemap)
					ImGui::TextDisabled("%ux%u cube, %u mips", component.Cubemap->GetFaceSize(),
						component.Cubemap->GetFaceSize(), component.Cubemap->GetMipLevels());
				else
					ImGui::TextDisabled("No cubemap loaded");
			});

		DrawComponent<StaticMeshComponent>("Static Mesh", entity, [](auto& component)
			{
				if (ImGuiUtils::DrawPathInput("Mesh Path", component.MeshPath, [&component]()->void
				{
					if (component.Mesh)
					{
						ImGui::Text("Vertices: %zu", component.Mesh->Vertices.size());
						ImGui::Text("Indices : %zu", component.Mesh->Indices.size());
						ImGui::Text("Submeshes: %zu", component.Mesh->Submeshes.size());
						ImGui::Text("Materials: %zu", component.Materials.size());
					}
					else
					{
						ImGui::TextDisabled("No mesh loaded");
					}
				}))
				{
					if (component.MeshPath.empty())
					{
						component.Mesh.reset();
						component.Materials.clear();
					}
					else
					{
						auto imported = MeshImporter::ImportStaticMesh(component.MeshPath);
						if (imported && imported->Mesh)
						{
							component.Mesh = imported->Mesh;
							component.Materials = imported->Materials;
							CANDY_INFO("Loaded static mesh '{}' ({} verts, {} submeshes)",
								component.MeshPath, component.Mesh->Vertices.size(), component.Mesh->Submeshes.size());
						}
						else
							CANDY_WARN("Could not import static mesh {0}", component.MeshPath);
					}
				}
			
				size_t submeshCount = component.Mesh ? component.Mesh->Submeshes.size() : 0;
				if (submeshCount == 0)
				{
					ImGui::TextDisabled("Load a mesh to assign materials");
					return;
				}
			
				// Keep MaterialPaths in sync with the submesh count.
				component.MaterialPaths.resize(submeshCount);

				for (size_t i = 0; i < submeshCount; i++)
				{
					const auto& submesh = component.Mesh->Submeshes[i];
					std::string label = submesh.Name.empty()
						? "Material " + std::to_string(i)
						: submesh.Name;

					if (ImGuiUtils::DrawPathInput(label, component.MaterialPaths[i]))
					{
						if (!component.MaterialPaths[i].empty() && i < component.Materials.size())
						{
							auto mat = MaterialCache::Get().Load(component.MaterialPaths[i]);
							if (mat)
								component.Materials[i] = mat;
						}
					}
				}
			});
	

		DrawComponent<SkeletalMeshComponent>("Skeletal Mesh", entity, [](auto& component)
			{
				// Mesh path + import (same pattern as Static Mesh).
				if (ImGuiUtils::DrawPathInput("Mesh Path", component.MeshPath, [&component]()->void
				{
					if (component.Mesh)
					{
						ImGui::Text("Vertices: %zu", component.Mesh->Vertices.size());
						ImGui::Text("Indices : %zu", component.Mesh->Indices.size());
						ImGui::Text("Joints  : %zu", component.Mesh->Skeleton.size());
						ImGui::Text("Clips   : %zu", component.Mesh->Clips.size());
					}
					else
					{
						ImGui::TextDisabled("No mesh loaded");
					}
				}))
				{
					if (component.MeshPath.empty())
					{
						component.Mesh.reset();
						component.Materials.clear();
						component.ClipName.clear();
						component.Time = 0.0f;
					}
					else
					{
						auto imported = MeshImporter::ImportSkeletalMesh(component.MeshPath);
						if (imported && imported->Mesh)
						{
							component.Mesh = imported->Mesh;
							component.Materials = imported->Materials;
							component.Time = 0.0f;
							if (component.ClipName.empty() && !imported->Mesh->Clips.empty())
								component.ClipName = imported->Mesh->Clips[0].Name;
							CANDY_INFO("Loaded skeletal mesh '{}' ({} verts, {} joints, {} clips)",
								component.MeshPath, component.Mesh->Vertices.size(),
								component.Mesh->Skeleton.size(), component.Mesh->Clips.size());
						}
						else
							CANDY_WARN("Could not import skeletal mesh {0}", component.MeshPath);
					}
				}

				if (!component.Mesh || component.Mesh->Skeleton.empty())
					return;

				// Animation clip selection.
				if (!component.Mesh->Clips.empty())
				{
					std::string preview = component.ClipName;
					if (ImGui::BeginCombo("Clip", preview.c_str()))
					{
						for (size_t i = 0; i < component.Mesh->Clips.size(); i++)
						{
							const bool selected = component.Mesh->Clips[i].Name == component.ClipName;
							if (ImGui::Selectable(component.Mesh->Clips[i].Name.c_str(), selected))
							{
								component.ClipName = component.Mesh->Clips[i].Name;
								component.Time = 0.0f;
							}
						}
						ImGui::EndCombo();
					}
				}

				// Playback controls.
				ImGuiUtils::DrawCheckbox("Play", component.Play);
				ImGuiUtils::DrawCheckbox("Loop", component.Loop);
				ImGuiUtils::DrawDragFloat("Speed", component.Speed, 0.05f, 0.0f, 10.0f);

				// Time timeline (scrubbing pauses playback).
				const AnimationClip* activeClip = nullptr;
				if (!component.ClipName.empty())
				{
					for (const auto& c : component.Mesh->Clips)
					{
						if (c.Name == component.ClipName)
						{
							activeClip = &c;
							break;
						}
					}
				}
				if (!activeClip && !component.Mesh->Clips.empty())
					activeClip = &component.Mesh->Clips[0];
				const float duration = activeClip ? activeClip->Duration : 0.0f;

				ImGuiUtils::DrawSliderFloat("Time", component.Time, 0.0f, std::max(duration, 0.001f), "%.2fs");

				ImGuiUtils::DrawCheckbox("Show Skeleton", component.ShowSkeleton);

				// Material overrides (one .mat path per submesh).
				size_t submeshCount = component.Mesh->Submeshes.size();
				component.MaterialPaths.resize(submeshCount);
				for (size_t i = 0; i < submeshCount; i++)
				{
					const auto& submesh = component.Mesh->Submeshes[i];
					std::string label = submesh.Name.empty()
						? "Material " + std::to_string(i)
						: submesh.Name;

					if (ImGuiUtils::DrawPathInput(label, component.MaterialPaths[i]))
					{
						if (!component.MaterialPaths[i].empty() && i < component.Materials.size())
						{
							auto mat = MaterialCache::Get().Load(component.MaterialPaths[i]);
							if (mat)
								component.Materials[i] = mat;
						}
					}
				}
			});

		DrawComponent<LightComponent>("Light", entity, [](auto& component)
			{
				const char* lightTypeStrings[] = { "Directional", "Point", "Spot" };
				int lightType = (int)component.Type;
				if (ImGuiUtils::DrawCombo("Type", lightTypeStrings, 3, lightType))
					component.Type = (LightComponent::LightType)lightType;

				ImGuiUtils::DrawColorEdit3("Color", component.Color);

				ImGuiUtils::DrawDragFloat("Intensity", component.Intensity, 0.05f, 0.0f);

				if (component.Type != LightComponent::LightType::Directional)
					ImGuiUtils::DrawDragFloat("Range", component.Range, 0.1f, 0.01f);

				if (component.Type == LightComponent::LightType::Spot)
				{
					ImGuiUtils::DrawDragFloat("Inner Cone Angle", component.InnerConeAngle, 0.5f, 0.0f, 89.0f, "%.1f deg");
					ImGuiUtils::DrawDragFloat("Outer Cone Angle", component.OuterConeAngle, 0.5f, 0.0f, 89.0f, "%.1f deg");
					if (component.OuterConeAngle < component.InnerConeAngle)
						component.OuterConeAngle = component.InnerConeAngle;
				}

				// Reserved for shadow mapping (not implemented yet).
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				ImGuiUtils::DrawCheckbox("Cast Shadows", component.CastShadows);
				ImGui::PopStyleVar();
				ImGui::TextDisabled("Shadow mapping is not implemented yet");
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
			if (ImGuiUtils::DrawPathInput("Script Path", component.ScriptPath))
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
			ImGuiUtils::DrawPathInput("Sound Path", component.SoundPath);
			ImGuiUtils::DrawDragFloat("Volume", component.Volume, 0.01f, 0.0f, 1.0f);
			ImGuiUtils::DrawCheckbox("Looping", component.Looping);
			ImGuiUtils::DrawCheckbox("Play On Start", component.PlayOnStart);
		});

		DrawComponent<UITextBlockComponent>("Text Blocks", entity, [](auto& component)
		{
			std::string toRemove;
			std::vector<std::pair<std::string, std::string>> toRename;
			float lineHeight = ImGui::GetFrameHeight();

			for (size_t i = 0; i < component.TextBlockOrder.size(); i++)
			{
				const auto& key = component.TextBlockOrder[i];
				auto& tb = component.TextBlockDatas[key];
				ImGui::PushID(static_cast<int>(i));
				bool open = ImGui::CollapsingHeader(key.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
				ImGui::SameLine(ImGui::GetContentRegionAvail().x - lineHeight);
				if (ImGui::Button("X", ImVec2{ lineHeight, lineHeight }))
					toRemove = key;
				if (open)
				{
					ImGui::Indent();
					std::string editKey = key;
					if (ImGuiUtils::DrawInputText("Name", editKey))
					{
						if (!editKey.empty() && editKey != key)
						{
							toRename.push_back({ key, editKey });
						}
					}

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
			{
				component.TextBlockDatas.erase(toRemove);
				auto it = std::find(component.TextBlockOrder.begin(), component.TextBlockOrder.end(), toRemove);
				if (it != component.TextBlockOrder.end())
					component.TextBlockOrder.erase(it);
			}
			for (auto& [oldKey, newKey] : toRename)
			{
				if (component.TextBlockDatas.contains(newKey))
					continue;
				auto nh = component.TextBlockDatas.extract(oldKey);
				if (!nh.empty())
				{
					nh.key() = newKey;
					component.TextBlockDatas.insert(std::move(nh));
				}
				auto it = std::find(component.TextBlockOrder.begin(), component.TextBlockOrder.end(), oldKey);
				if (it != component.TextBlockOrder.end())
					*it = newKey;
			}

			ImGui::Separator();
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x * 0.5f - 60.0f);
			if (ImGui::Button("+ TextBlock", ImVec2{ 120.0f, 0.0f }))
			{
				static uint32_t s_TextBlockCounter = 0;
				std::string key = "TextBlock_" + std::to_string(++s_TextBlockCounter);
				component.TextBlockDatas[key] = TextBlockUIData();
				component.TextBlockOrder.push_back(key);
			}
		});

		DrawComponent<UIButtonComponent>("Buttons", entity, [](auto& component)
		{
			std::string toRemove;
			std::vector<std::pair<std::string, std::string>> toRename;
			float lineHeight = ImGui::GetFrameHeight();

			for (size_t i = 0; i < component.ButtonOrder.size(); i++)
			{
				const auto& key = component.ButtonOrder[i];
				auto& btn = component.ButtonDatas[key];
				ImGui::PushID(static_cast<int>(i));
				bool open = ImGui::CollapsingHeader(key.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
				ImGui::SameLine(ImGui::GetContentRegionAvail().x - lineHeight);
				if (ImGui::Button("X", ImVec2{ lineHeight, lineHeight }))
					toRemove = key;
				if (open)
				{
					ImGui::Indent();

					std::string editKey = key;
					if (ImGuiUtils::DrawInputText("Name", editKey))
					{
						if (!editKey.empty() && editKey != key)
						{
							toRename.push_back({ key, editKey });
						}
					}
					ImGuiUtils::DrawInputText("Text", btn.Text);
					ImGuiUtils::DrawDragFloat("Font Size", btn.FontSize, 1.0f, 1.0f, 200.0f);
					ImGuiUtils::DrawDragFloat2("Size", btn.Size);
					ImGuiUtils::DrawDragFloat2("Position", btn.Position);
					ImGuiUtils::DrawInputText("OnClick", btn.OnClick);
					ImGuiUtils::DrawCheckbox("Visible", btn.Visible);

					ImGui::Unindent();
				}
				ImGui::PopID();
			}
			if (!toRemove.empty())
			{
				component.ButtonDatas.erase(toRemove);
				auto it = std::find(component.ButtonOrder.begin(), component.ButtonOrder.end(), toRemove);
				if (it != component.ButtonOrder.end())
					component.ButtonOrder.erase(it);
			}
			for (auto& [oldKey, newKey] : toRename)
			{
				if (component.ButtonDatas.contains(newKey))
					continue;
				auto nh = component.ButtonDatas.extract(oldKey);
				if (!nh.empty())
				{
					nh.key() = newKey;
					component.ButtonDatas.insert(std::move(nh));
				}
				auto it = std::find(component.ButtonOrder.begin(), component.ButtonOrder.end(), oldKey);
				if (it != component.ButtonOrder.end())
					*it = newKey;
			}

			ImGui::Separator();
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x * 0.5f - 60.0f);
			if (ImGui::Button("+ Button", ImVec2{ 120.0f, 0.0f }))
			{
				static uint32_t s_ButtonCounter = 0;
				std::string key = "Button_" + std::to_string(++s_ButtonCounter);
				component.ButtonDatas[key] = ButtonUIData();
				component.ButtonOrder.push_back(key);
			}
		});
	}

}