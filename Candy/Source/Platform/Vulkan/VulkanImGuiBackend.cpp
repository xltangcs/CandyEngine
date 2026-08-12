#include "CandyPCH.h"
#include <Windows.h>

#include "Platform/Vulkan/VulkanImGuiBackend.h"

#include "Runtime/Core/Application.h"
#include "Runtime/Core/Log.h"
#include "Platform/Vulkan/VulkanGraphicsContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanSwapChain.h"
#include "Platform/Windows/WindowsWindow.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#include <backends/imgui_impl_vulkan.h>

namespace Candy {

	void VulkanImGuiBackend::Init(GLFWwindow* window)
	{
		ImGui_ImplGlfw_InitForVulkan(window, true);
	}

	void VulkanImGuiBackend::InitContext()
	{
		auto* gfxCtx = static_cast<VulkanGraphicsContext*>(
			static_cast<WindowsWindow*>(&Application::Get().GetWindow())->GetGraphicsContext());
		auto* vkDev  = gfxCtx->GetDevice();
		auto* vkSC   = gfxCtx->GetSwapChain();

		VkDevice device = vkDev->GetVkDevice();

		// Create descriptor pool for ImGui
		{
			VkDescriptorPoolSize poolSizes[] = {
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16 },
			};
			VkDescriptorPoolCreateInfo dpci = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
			dpci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			dpci.maxSets       = 16;
			dpci.poolSizeCount = 1;
			dpci.pPoolSizes    = poolSizes;

			VkDescriptorPool pool;
			vkDev->fnCreateDescriptorPool(device, &dpci, nullptr, &pool);
			m_DescriptorPool = pool;
		}

		ImGui_ImplVulkan_InitInfo initInfo = {};
		initInfo.Instance        = vkDev->GetVkInstance();
		initInfo.PhysicalDevice  = vkDev->GetVkPhysicalDevice();
		initInfo.Device          = device;
		initInfo.QueueFamily     = vkDev->GetGraphicsQueueFamilyIndex();
		initInfo.Queue           = vkDev->GetVkQueue();
		initInfo.DescriptorPool  = static_cast<VkDescriptorPool>(m_DescriptorPool);
		initInfo.MinImageCount   = 2;
		initInfo.ImageCount      = 2;
		initInfo.PipelineInfoMain.RenderPass = vkSC->GetRenderPass();
		initInfo.PipelineInfoMain.Subpass = 0;

		// Load Vulkan functions for ImGui
		ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, [](const char* name, void* userData) -> PFN_vkVoidFunction {
			auto* dev = static_cast<VulkanDevice*>(userData);
			return dev->GetProcAddr(name);
		}, vkDev);

		ImGui_ImplVulkan_Init(&initInfo);
	}

	void VulkanImGuiBackend::ShutdownContext()
	{
		ImGui_ImplVulkan_Shutdown();
	}

	void VulkanImGuiBackend::Shutdown()
	{
		ImGui_ImplGlfw_Shutdown();

		auto* gfxCtx = static_cast<VulkanGraphicsContext*>(
			static_cast<WindowsWindow*>(&Application::Get().GetWindow())->GetGraphicsContext());
		if (gfxCtx && gfxCtx->GetDevice() && m_DescriptorPool)
		{
			gfxCtx->GetDevice()->fnDestroyDescriptorPool(
				gfxCtx->GetDevice()->GetVkDevice(),
				static_cast<VkDescriptorPool>(m_DescriptorPool), nullptr);
		}
		m_DescriptorPool = nullptr;
	}

	void VulkanImGuiBackend::NewFrame()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
	}

	void VulkanImGuiBackend::NewFrameGameUI()
	{
		ImGui_ImplVulkan_NewFrame();
	}

	void VulkanImGuiBackend::RenderDrawData(ImDrawData* drawData, Framebuffer* /*target*/)
	{
		if (!drawData || drawData->CmdListsCount == 0) return;
		ImGui_ImplVulkan_RenderDrawData(drawData, VK_NULL_HANDLE, VK_NULL_HANDLE);
	}
}
