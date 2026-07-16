#include <Candy.h>
#include <Candy/Core/EntryPoint.h>
#include <Candy/Core/FileSystem.h>
#include <Candy/Utils/PlatformUtils.h>

#include "GameLayer.h"

namespace Candy {

	class CandyGameApp : public Application
	{
	public:
		CandyGameApp(int argc, char** argv)
			: Application("Candy Game")
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
