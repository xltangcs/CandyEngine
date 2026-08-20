#include "ConsolePanel.h"

#include "CandyPCH.h"
#include "Runtime/Core/Log.h"
#include "Runtime/Core/ConsoleLogSink.h"

#include <imgui/imgui.h>

namespace Candy {

	static bool ToggleFilterButton(const char* label, bool* value)
	{
		bool changed = false;
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Border, *value ? ImVec4(0.6f, 0.7f, 1.0f, 1.0f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		if (ImGui::Checkbox(label, value))
			changed = true;
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
		return changed;
	}

	static ImVec4 LevelColor(spdlog::level::level_enum level)
	{
		switch (level)
		{
		case spdlog::level::trace:    return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
		case spdlog::level::debug:    return ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
		case spdlog::level::warn:     return ImVec4(1.0f, 0.85f, 0.30f, 1.0f);
		case spdlog::level::err:      return ImVec4(1.0f, 0.40f, 0.40f, 1.0f);
		case spdlog::level::critical: return ImVec4(1.0f, 0.20f, 0.20f, 1.0f);
		case spdlog::level::info:
		default:                      return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}

	void ConsolePanel::OnImGuiRender(bool* open)
	{
		ImGui::Begin("Console", open);

		// ---- Toolbar ----
		if (ImGui::Button("Clear"))
			Log::GetConsoleSink()->Clear();
		ImGui::SameLine();

		static bool autoScroll = true;
		ImGui::Checkbox("Auto-scroll", &autoScroll);
		ImGui::SameLine(0.0f, 12.0f);

		static bool showTrace = true, showDebug = true, showInfo = true, showWarn = true, showError = true;
		ToggleFilterButton("Trace", &showTrace);
		ImGui::SameLine();
		ToggleFilterButton("Debug", &showDebug);
		ImGui::SameLine();
		ToggleFilterButton("Info", &showInfo);
		ImGui::SameLine();
		ToggleFilterButton("Warn", &showWarn);
		ImGui::SameLine();
		ToggleFilterButton("Error", &showError);

		ImGui::Separator();

		// ---- Log entries ----
		ImGui::BeginChild("##LogScrollRegion", ImVec2(0.0f, 0.0f), false);

		auto entries = Log::GetConsoleSink()->GetEntries();

		std::vector<const ConsoleLogSink::Entry*> filtered;
		filtered.reserve(entries.size());
		for (const auto& e : entries)
		{
			bool visible = (showTrace && e.Level == spdlog::level::trace)
				|| (showDebug && e.Level == spdlog::level::debug)
				|| (showInfo && e.Level == spdlog::level::info)
				|| (showWarn && e.Level == spdlog::level::warn)
				|| (showError && (e.Level == spdlog::level::err || e.Level == spdlog::level::critical));
			if (visible)
				filtered.push_back(&e);
		}

		ImGuiListClipper clipper;
		clipper.Begin((int)filtered.size());
		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			{
				const auto& e = *filtered[i];
				ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(e.Level));
				ImGui::TextUnformatted(e.Message.c_str());
				ImGui::PopStyleColor();
			}
		}
		clipper.End();

		if (autoScroll)
		{
			bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f;
			if (atBottom || ImGui::GetScrollMaxY() == 0.0f)
				ImGui::SetScrollHereY(1.0f);
		}

		ImGui::EndChild();

		ImGui::End();
	}

}
