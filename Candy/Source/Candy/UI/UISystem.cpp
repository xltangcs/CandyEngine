#include "CandyPCH.h"
#include "Candy/UI/UISystem.h"
#include "Candy/Scene/Scene.h"
#include "Candy/Scene/Entity.h"
#include "Candy/Scene/Components.h"
#include "Candy/Scripting/ScriptSystem.h"

#include <imgui.h>

namespace Candy {

	void UISystem::Render(Scene& scene, const ImVec2& viewportOrigin)
	{
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

					ImVec2 pos(viewportOrigin.x + tb.Position.x,
					           viewportOrigin.y + tb.Position.y);
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

					ImVec2 pos(viewportOrigin.x + btn.Position.x,
					           viewportOrigin.y + btn.Position.y);
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

					// Hit detection
					ImGui::SetCursorScreenPos(pos);
					ImGui::PushID(&btn);
					bool clicked = ImGui::InvisibleButton("##btn", size);
					ImGui::PopID();

					if (clicked && !btn.OnClick.empty())
						ScriptSystem::Get().CallFunction(entity, btn.OnClick);
				}
			}
		}
	}

}
