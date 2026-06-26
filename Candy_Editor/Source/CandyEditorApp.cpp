#include <Candy.h>
#include <Candy/Core/EntryPoint.h>

#include "EditorLayer.h"

namespace Candy {

	class CandyEditor : public Application
	{
	public:
		CandyEditor()
			: Application("Candy Editor")
		{
			PushLayer(new EditorLayer());
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