#include <Candy.h>
#include <Runtime/Core/EntryPoint.h>
#include <Runtime/Core/FileSystem.h>
#include <Runtime/Utils/PlatformUtils.h>
#include <Runtime/Project/ProjectSettings.h>

#include "GameLayer.h"

namespace Candy {

	class CandyGameApp : public Application
	{
	public:
		CandyGameApp(int argc, char** argv)
			: Application("Candy Game", 1280, 720, false, false)
		{
			if (argc >= 2)
			{
				std::filesystem::path projectPath = argv[1];
				if (projectPath.extension() != ".candyproj")
				{
					CANDY_CORE_ERROR("Not a Candy project file: {0}", projectPath.string());
					Close();
					return;
				}
				LoadProject(projectPath);
				ProjectSettings::Get().Load();
			}
			else
			{
				// Standalone mode: find .pak next to executable
				std::string exeDir = GetExecutableDirectory();
				std::filesystem::path pakPath;
				for (auto& entry : std::filesystem::directory_iterator(exeDir))
				{
					if (entry.path().extension() == ".pak")
					{
						pakPath = entry.path();
						break;
					}
				}

				if (pakPath.empty())
				{
					CANDY_CORE_ERROR("No .pak file found next to executable: {0}", exeDir);
					Close();
					return;
				}

				CANDY_CORE_INFO("Found pak: {0}", pakPath.string());
				FileSystem::Get().Mount("/", pakPath);

				LoadProjectFromVfs("/project.candyproj");
			}

			// Apply window size from ProjectSettings (window is non-resizable in game mode)
			{
				auto& ps = ProjectSettings::Get();
				GetWindow().SetSize(ps.DefaultWidth, ps.DefaultHeight);
			}

			// VFS .pak mount happens after Application base ctor (which already ran ImGuiLayer::OnAttach).
			// Reload fonts now so they can be read from VFS.
			GetImGuiLayer()->ReloadFontsFromVfs();
			// Standalone game mode: editorContext would otherwise load Saved/imgui.ini and render
			// leftover editor windows on top of the game UI (Pass 3 overlay). Disable it.
			GetImGuiLayer()->DisableEditorChrome();

			PushLayer(new GameLayer());
		}

		~CandyGameApp()
		{
		}
	};

}

Candy::Application* Candy::CreateApplication(int argc, char** argv)
{
	return new CandyGameApp(argc, argv);
}
