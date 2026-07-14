#include "ProjectManagerLayer.h"
#include "EditorLayer.h"

#include <imgui/imgui.h>
#include "Candy/Utils/PlatformUtils.h"
#include "Candy/Project/ProjectSerializer.h"
#include "Candy/Project/RecentProjects.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>

#include "GLFW/glfw3.h"

namespace Candy {
		
	ProjectManagerLayer::ProjectManagerLayer()
		: Layer("ProjectManager")
	{
	}

	void ProjectManagerLayer::OnAttach()
	{
		auto& editorState = EditorState::Get();
		editorState.Load();
		
		auto& editorSettings = EditorSettings::Get();
		editorSettings.Load();
		ImGuiLayer::RebuildFont(editorSettings.m_FontPath);

		GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		if (editorState.WindowMaximized)
			glfwMaximizeWindow(window);
		else
			glfwSetWindowSize(window, editorState.WindowWidth, editorState.WindowHeight);
		
		m_RecentProjects = RecentProjects::Load();
	}

	void ProjectManagerLayer::OnDetach()
	{
		EditorState::Get().Save();
	}

	void ProjectManagerLayer::OnImGuiRender()
	{
		ImGui::PushFont(nullptr, EditorSettings::Get().m_FontSize);
		
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

		ImGui::Begin("Project Manager", nullptr, windowFlags);
		ImGui::PopStyleVar(2);

		float windowWidth = ImGui::GetWindowWidth();
		float windowHeight = ImGui::GetWindowHeight();
		float padding = windowWidth * 0.05f;
		float leftWidth = windowWidth * 0.58f;
		float rightWidth = windowWidth * 0.32f;

		// ---- Title ----
		ImGui::SetCursorPosX(padding);
		ImGui::SetCursorPosY(padding * 0.5f);
		ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.2f, 1.0f), "Candy Engine");
		ImGui::SameLine();
		ImGui::Text(" -  Project Manager");
		ImGui::Separator();
		ImGui::Spacing();

		// ---- Left: Recent Projects ----
		float leftStartY = ImGui::GetCursorPosY();
		ImGui::SetCursorPosX(padding);
		ImGui::BeginChild("RecentProjects", ImVec2(leftWidth, 0), true);

		ImGui::Text("Recent Projects");
		ImGui::Separator();

		if (m_RecentProjects.empty())
		{
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20);
			ImGui::TextDisabled("No recent projects found.");
		}
		else
		{
			for (auto it = m_RecentProjects.rbegin(); it != m_RecentProjects.rend(); ++it)
			{
				const auto& entry = *it;

				if (!std::filesystem::exists(entry.Path))
					continue;

				ImGui::PushID(entry.Path.c_str());

				ImVec2 itemMin = ImGui::GetCursorScreenPos();
				ImVec2 itemMax = ImVec2(itemMin.x + leftWidth - 30, itemMin.y + 50);

				bool hovered = ImGui::IsMouseHoveringRect(itemMin, itemMax);

				// Hover highlight
				if (hovered)
				{
					ImGui::GetWindowDrawList()->AddRectFilled(
						itemMin, itemMax,
						IM_COL32(60, 60, 80, 180), 4.0f);
				}

				ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetScrollY());

				ImGui::TextUnformatted(entry.Name.c_str());
				ImGui::TextDisabled("%s", entry.Path.c_str());

				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && hovered)
				{
					Application::Get().LoadProject(entry.Path);
					if (Application::Get().GetProject())
					{
						Application::Get().SchedulePopLayer(this);
						Application::Get().SchedulePushLayer(new EditorLayer());
					}
				}

				ImGui::Dummy(ImVec2(0, 12));
				ImGui::PopID();
			}
		}

		ImGui::EndChild();

		// ---- Right: Actions ----
		ImGui::SameLine();
		float rightStartX = padding + leftWidth + 20.0f;
		ImGui::SetCursorPosX(rightStartX);
		ImGui::SetCursorPosY(leftStartY);
		ImGui::BeginChild("Actions", ImVec2(rightWidth, 0), true);

		ImGui::Text("Get Started");
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("New Project", ImVec2(-1, 48)))
		{
			NewProject();
		}

		ImGui::Spacing();

		if (ImGui::Button("Open Project", ImVec2(-1, 48)))
		{
			OpenProject();
		}

		ImGui::EndChild();

		// ---- New Project Dialog ----
		if (m_ShowNewProjectDialog)
			RenderNewProjectDialog();

		ImGui::End();

		ImGui::PopFont();
	}

	void ProjectManagerLayer::OpenProject()
	{
		std::string filepath = FileDialogs::OpenFile("Candy Project (*.candyproj)\0*.candyproj\0");
		if (filepath.empty())
			return;

		Application::Get().LoadProject(filepath);
		if (Application::Get().GetProject())
		{
			auto project = Application::Get().GetProject();
			RecentProjects::Add(project->GetName(), project->GetProjectFileName().string());
			Application::Get().SchedulePopLayer(this);
			Application::Get().SchedulePushLayer(new EditorLayer());
		}
	}

	void ProjectManagerLayer::NewProject()
	{
		memset(m_NewProjectInfo.Name, 0, sizeof(m_NewProjectInfo.Name));
		strcpy_s(m_NewProjectInfo.Name, "New Game Project");
		memset(m_NewProjectInfo.Path, 0, sizeof(m_NewProjectInfo.Path));
		const char* home = getenv("USERPROFILE");
		if (home)
		{
			std::string docs = (std::filesystem::path(home) / "Documents").string();
			strcpy_s(m_NewProjectInfo.Path, docs.c_str());
		}
		m_ShowNewProjectDialog = true;
	}

	void ProjectManagerLayer::RenderNewProjectDialog()
	{
		ImGui::SetNextWindowSize(ImVec2(540, 280));
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);

		ImGui::Begin("Create New Project", &m_ShowNewProjectDialog,
			ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse);

		// Project Name
		ImGui::Text("Project Name:");
		ImGui::SameLine();
		ImGui::InputText("##name", m_NewProjectInfo.Name, sizeof(m_NewProjectInfo.Name));

		ImGui::Spacing();

		// Project Path
		ImGui::Text("Project Path:");
		ImGui::SameLine();
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 100);
		ImGui::InputText("##path", m_NewProjectInfo.Path, sizeof(m_NewProjectInfo.Path));
		ImGui::PopItemWidth();
		
		ImGui::SameLine();
		if (ImGui::Button("..."))
		{
			std::string dir = FileDialogs::OpenFolder();
			if (!dir.empty())
				strcpy_s(m_NewProjectInfo.Path, dir.c_str());
		}
		
		ImGui::Separator();
		ImGui::Spacing();

		// Buttons
		float buttonWidth = 100;
		float totalButtonWidth = buttonWidth * 2 + ImGui::GetStyle().ItemSpacing.x;
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - totalButtonWidth) * 0.5f);

		if (ImGui::Button("Create", ImVec2(buttonWidth, 30)))
		{
			Application::Get().CreateProject(m_NewProjectInfo.Path, m_NewProjectInfo.Name);
			if (Application::Get().GetProject())
			{
				auto project = Application::Get().GetProject();
				RecentProjects::Add(project->GetName(), project->GetProjectFileName().string());
				m_ShowNewProjectDialog = false;
				Application::Get().SchedulePopLayer(this);
				Application::Get().SchedulePushLayer(new EditorLayer());
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(buttonWidth, 30)))
		{
			m_ShowNewProjectDialog = false;
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

}
