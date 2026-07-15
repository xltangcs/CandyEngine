#include <Candy.h>
#include <Candy/Core/EntryPoint.h>

#include "GameLayer.h"

namespace Candy {

	class CandyGameApp : public Application
	{
	public:
		CandyGameApp(int argc, char** argv)
			: Application("Candy Game")
		{
			if (argc < 2)
			{
				CANDY_CORE_ERROR("Usage: CandyGame.exe <project.candyproj>");
				Close();
				return;
			}

			std::filesystem::path projectPath = argv[1];
			if (projectPath.extension() != ".candyproj")
			{
				CANDY_CORE_ERROR("Not a Candy project file: {0}", projectPath.string());
				Close();
				return;
			}

			LoadProject(projectPath);
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
