#include "CandyPCH.h"
#include "Platform/Vulkan/VulkanGraphicsContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanSwapChain.h"
#include "Runtime/Core/Log.h"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Candy {

	VulkanGraphicsContext::VulkanGraphicsContext(const WindowHandle& handle)
		: m_Window(static_cast<GLFWwindow*>(handle.Native))
	{
	}

	VulkanGraphicsContext::~VulkanGraphicsContext()
	{
		m_SwapChain.reset();
		m_Device.reset();
		CANDY_CORE_INFO("VulkanGraphicsContext: destroyed");
	}

	void VulkanGraphicsContext::Init()
	{
		CANDY_CORE_INFO("VulkanGraphicsContext: initializing...");

		m_Device = std::make_unique<VulkanDevice>();
		if (!m_Device->IsInitialized())
		{
			CANDY_CORE_ERROR("VulkanGraphicsContext: VulkanDevice initialization failed");
			m_Device.reset();
			return;
		}

		HWND hwnd = glfwGetWin32Window(m_Window);
		if (!hwnd)
		{
			CANDY_CORE_ERROR("VulkanGraphicsContext: failed to get HWND");
			return;
		}

		int width, height;
		glfwGetFramebufferSize(m_Window, &width, &height);

		SwapChainDesc scDesc;
		scDesc.Window      = WindowHandle{ static_cast<void*>(hwnd) };
		scDesc.Width       = static_cast<uint32_t>(width);
		scDesc.Height      = static_cast<uint32_t>(height);
		scDesc.BufferCount = 2;
		scDesc.VSync       = true;

		Ref<RHISwapChain> swapChain = m_Device->CreateSwapChain(scDesc);
		if (!swapChain)
		{
			CANDY_CORE_ERROR("VulkanGraphicsContext: swap chain creation failed");
			return;
		}

		// VulkanSwapChain* is the concrete type, release from Ref, wrap in unique_ptr
		m_SwapChain.reset(static_cast<VulkanSwapChain*>(swapChain.release()));

		CANDY_CORE_INFO("VulkanGraphicsContext: initialized ({}x{})", width, height);
	}

	void VulkanGraphicsContext::SwapBuffers()
	{
		// In Vulkan, Present happens during command submission (queue.Present).
		// SwapBuffers is a no-op for Vulkan.
	}

} // namespace Candy
