#pragma once

#include "Runtime/Imgui/ImGuiBackend.h"

namespace Candy {

	// [EXPERIMENTAL — FROZEN] Vulkan backend is parked; see AGENTS.md.
	// VulkanImGuiBackend — ImGui_ImplVulkan adapter (incomplete: swap-chain
	// render pass plumbing was never finished, kept compiling only).
	class VulkanImGuiBackend : public ImGuiBackend
	{
	public:
		void Init(GLFWwindow* window) override;
		void InitContext() override;
		void ShutdownContext() override;
		void Shutdown() override;

		bool SupportsPlatformWindows() const override { return false; }

		void NewFrame() override;
		void NewFrameGameUI() override;
		void RenderDrawData(ImDrawData* drawData, Framebuffer* target) override;

	private:
		void* m_DescriptorPool = nullptr; // VkDescriptorPool
	};
}
