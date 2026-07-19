#include "CandyPCH.h"
#include "Runtime/UI/UISystem.h"
#include "Runtime/Scene/Scene.h"
#include "Runtime/Scene/Entity.h"
#include "Runtime/Scene/Components.h"
#include "Runtime/Scripting/ScriptSystem.h"

#include <imgui.h>

namespace Candy {

	void UISystem::RenderUI(Scene& scene)
	{
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 displaySize = io.DisplaySize;

		// Full-screen window to contain game UI elements
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(displaySize);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::Begin("##GameUI", nullptr,
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_NoInputs);
		ImGui::PopStyleVar(2);

		auto* drawList = ImGui::GetWindowDrawList();

		// Render TextBlocks
		{
			auto view = scene.GetAllEntitiesWith<UITextBlockComponent>();
			for (auto e : view)
			{
				Entity entity{ e, &scene };
				auto& comp = entity.GetComponent<UITextBlockComponent>();
				for (auto& [key, tb] : comp.TextBlocks)
				{
					if (!tb.Visible) continue;

					ImVec2 pos(tb.Position.x, tb.Position.y);
					ImU32 col = IM_COL32(
						(int)(tb.Color.r * 255.0f),
						(int)(tb.Color.g * 255.0f),
						(int)(tb.Color.b * 255.0f),
						(int)(tb.Color.a * 255.0f));

					drawList->AddText(nullptr, tb.FontSize, pos, col, tb.Text.c_str());
				}
			}
		}

		// Render Buttons
		{
			auto view = scene.GetAllEntitiesWith<UIButtonComponent>();
			for (auto e : view)
			{
				Entity entity{ e, &scene };
				auto& comp = entity.GetComponent<UIButtonComponent>();
				for (auto& [key, btn] : comp.Buttons)
				{
					if (!btn.Visible) continue;

					ImVec2 pos(btn.Position.x, btn.Position.y);
					ImVec2 size(btn.Size.x, btn.Size.y);

					// Draw button background
					drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
						IM_COL32(60, 60, 60, 200), 4.0f);

					// Draw centered text
					ImVec2 textSize = ImGui::CalcTextSize(btn.Text.c_str());
					drawList->AddText(
						ImVec2(pos.x + (size.x - textSize.x) * 0.5f,
						       pos.y + (size.y - textSize.y) * 0.5f),
						IM_COL32(255, 255, 255, 255), btn.Text.c_str());

					// Hit detection (need manual hit test since window has NoInputs)
					ImVec2 mousePos = io.MousePos;
					bool hovered = (mousePos.x >= pos.x && mousePos.x <= pos.x + size.x &&
					                mousePos.y >= pos.y && mousePos.y <= pos.y + size.y);
					if (hovered && io.MouseClicked[0])
					{
						if (!btn.OnClick.empty())
							ScriptSystem::Get().CallFunction(entity, btn.OnClick);
						io.MouseClicked[0] = false;
					}
				}
			}
		}

		ImGui::End();
	}

}
