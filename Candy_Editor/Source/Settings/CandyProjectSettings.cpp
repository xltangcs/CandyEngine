#include "CandyPCH.h"
#include "CandyProjectSettings.h"

#include <imgui/imgui.h>
#include "CandyEditorState.h"

namespace Candy {

	CandyProjectSettings& CandyProjectSettings::Get()
	{
		static CandyProjectSettings instance;
		return instance;
	}

	void CandyProjectSettings::OnImGuiRender()
	{
		ImGui::Begin("Project Settings", &CandyEditorState::Get().ShowProjectSettings);
		ImGui::End();
	}

	void CandyProjectSettings::Save()
	{
		// TODO
	}

	void CandyProjectSettings::Load()
	{
		// TODO
	}

}
