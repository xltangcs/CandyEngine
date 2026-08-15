#pragma once

#include "Candy.h"
#include "Runtime/Project/RecentProjects.h"

namespace Candy {

	class ProjectManagerLayer : public Layer
	{
	public:
		ProjectManagerLayer();
		virtual ~ProjectManagerLayer() = default;

		// The Project Manager is a compact launcher window of its own (like
		// Godot's), distinct from the persisted editor geometry in EditorState.
		static constexpr uint32_t ProjectManagerWidth = 1024;
		static constexpr uint32_t ProjectManagerHeight = 640;

		virtual void OnAttach() override;
		virtual void OnImGuiRender() override;

	private:
		struct NewProjectInfo
		{
			char Name[256] = "";
			char Path[4096] = "";
		};

		void OpenProject();
		void NewProject();
		void RenderNewProjectDialog();

		std::vector<RecentProjectEntry> m_RecentProjects;

		bool m_ShowNewProjectDialog = false;
		NewProjectInfo m_NewProjectInfo;
	};

}
