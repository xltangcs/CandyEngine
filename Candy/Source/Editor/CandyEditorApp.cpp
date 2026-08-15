#include <Candy.h>
#include <Runtime/Core/EntryPoint.h>
#include <Runtime/Project/RecentProjects.h>

#include "Layer/EditorLayer.h"
#include "Layer/ProjectManagerLayer.h"
#include "Setting/EditorSettings.h"
#include "Setting/EditorState.h"

#include <yaml-cpp/yaml.h>
#include <filesystem>

namespace Candy {

	/// Startup options resolved *before* the `Application` base ctor runs:
	/// the RHI backend must be in place before Window/GraphicsContext are
	/// created, and the window is created at its final size/maximized state so
	/// no post-create resize happens at startup. The Project Manager gets its
	/// own compact launcher window; the editor restores the persisted geometry.
	struct StartupOptions
	{
		std::string RendererAPI = "D3D12";
		uint32_t Width = 1280;
		uint32_t Height = 720;
		bool Maximized = false;
		bool AutoOpenProject = false;
		std::filesystem::path LastProjectPath;
	};

	static const StartupOptions& ResolveStartupOptions()
	{
		static const StartupOptions options = []()
		{
			StartupOptions opts;

			EditorSettings::Get().Load();
			EditorState::Get().Load();

			auto recents = RecentProjects::Load();
			opts.AutoOpenProject = EditorSettings::Get().m_AutoOpenLastProject && !recents.empty();
			if (!opts.AutoOpenProject)
			{
				// Project Manager: compact launcher window of its own.
				opts.Width = ProjectManagerLayer::ProjectManagerWidth;
				opts.Height = ProjectManagerLayer::ProjectManagerHeight;
				return opts;
			}

			// Editor: restore the persisted window geometry.
			opts.Width = (uint32_t)EditorState::Get().WindowWidth;
			opts.Height = (uint32_t)EditorState::Get().WindowHeight;
			opts.Maximized = EditorState::Get().WindowMaximized;
			opts.LastProjectPath = recents[0].Path;

			// Peek the auto-opened last project's `.candyproj` YAML directly
			// and return its `RendererAPI` field (fallback: "D3D12"). This is
			// what makes "restart to apply" in Project Settings actually apply
			// without recompiling.
			std::error_code ec;
			if (std::filesystem::exists(opts.LastProjectPath, ec))
			{
				try
				{
					YAML::Node data = YAML::LoadFile(opts.LastProjectPath.string());
					if (auto proj = data["Project"])
						if (proj["RendererAPI"])
							opts.RendererAPI = proj["RendererAPI"].as<std::string>();
				}
				catch (const std::exception&)
				{
					// malformed yaml — fall back to engine default
				}
			}
			return opts;
		}();
		return options;
	}

	class CandyEditor : public Application
	{
	public:
		CandyEditor()
			: Application("Candy Engine",
				ResolveStartupOptions().Width,
				ResolveStartupOptions().Height,
				true, true,
				ResolveStartupOptions().RendererAPI,
				ResolveStartupOptions().Maximized)
		{
			if (ResolveStartupOptions().AutoOpenProject)
			{
				LoadProject(ResolveStartupOptions().LastProjectPath);
				PushLayer(new EditorLayer());
				return;
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