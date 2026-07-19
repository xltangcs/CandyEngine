#pragma once

#include "Candy.h"
#include "Runtime/Project/RecentProjects.h"

namespace Candy {

	class ProjectManagerLayer : public Layer
	{
	public:
		ProjectManagerLayer();
		virtual ~ProjectManagerLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;
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
