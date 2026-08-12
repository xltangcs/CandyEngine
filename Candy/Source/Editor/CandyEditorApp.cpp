#include <Candy.h>
#include <Runtime/Core/EntryPoint.h>
#include <Runtime/Project/RecentProjects.h>

#include "Layer/EditorLayer.h"
#include "Layer/ProjectManagerLayer.h"
#include "Setting/EditorSettings.h"

#include <yaml-cpp/yaml.h>
#include <filesystem>

namespace Candy {

	/// Peek the auto-opened last project's `.candyproj` YAML directly and
	/// return its `RendererAPI` field.  Falls back to the engine default
	/// ("D3D12") when no auto-open target exists or the file is missing the
	/// key.  Runs *before* the `Application` base ctor — so the chosen
	/// backend is in place by the time Window / GraphicsContext are created.
	/// This is what makes "restart to apply" in Project Settings actually
	/// apply without recompiling.
	static std::string ResolveInitialRendererAPI()
	{
		EditorSettings::Get().Load();
		if (EditorSettings::Get().m_AutoOpenLastProject)
		{
			auto recents = RecentProjects::Load();
			if (!recents.empty())
			{
				const std::filesystem::path& projectFile = recents[0].Path;
				std::error_code ec;
				if (std::filesystem::exists(projectFile, ec))
				{
					try
					{
						YAML::Node data = YAML::LoadFile(projectFile.string());
						if (auto proj = data["Project"])
							if (proj["RendererAPI"])
								return proj["RendererAPI"].as<std::string>();
					}
					catch (const std::exception&)
					{
						// malformed yaml — fall back to engine default
					}
				}
			}
		}
		return "D3D12";
	}

	class CandyEditor : public Application
	{
	public:
		CandyEditor()
			: Application("Candy Engine", 1280, 720, true, true, ResolveInitialRendererAPI())
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