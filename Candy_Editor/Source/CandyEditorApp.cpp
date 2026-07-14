#include <Candy.h>
#include <Candy/Core/EntryPoint.h>

#include "EditorLayer.h"
#include "Panels/ProjectManagerLayer.h"

namespace Candy {

	class CandyEditor : public Application
	{
	public:
		CandyEditor()
			: Application("Candy Engine")
		{
#if 0
			PushLayer(new EditorLayer());
#else
			PushLayer(new ProjectManagerLayer());
#endif
		}

		~CandyEditor()
		{
		}
	};

	Application* CreateApplication()
	{
		return new CandyEditor();
	}

}