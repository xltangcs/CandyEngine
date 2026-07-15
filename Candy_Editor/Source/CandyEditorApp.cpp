#include <Candy.h>
#include <Candy/Core/EntryPoint.h>
#include <Candy/Project/RecentProjects.h>

#include "EditorLayer.h"
#include "Panels/ProjectManagerLayer.h"
#include "Settings/EditorSettings.h"

namespace Candy {

	class CandyEditor : public Application
	{
	public:
		CandyEditor()
			: Application("Candy Engine")
		{
			EditorSettings::Get().Load();

			if (EditorSettings::Get().m_AutoOpenLastProject)
			{
				auto recentProjects = RecentProjects::Load();
				if (!recentProjects.empty())
				{
					LoadProject(recentProjects[0].Path);
					PushLayer(new EditorLayer());
					return;
				}
			}
			PushLayer(new ProjectManagerLayer());
		}

		~CandyEditor()
		{
		}
	};

	Application* CreateApplication(int argc, char** argv)
	{
		return new CandyEditor();
	}

}