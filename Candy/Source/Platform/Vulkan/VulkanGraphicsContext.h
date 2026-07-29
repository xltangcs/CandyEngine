#pragma once

#include "Runtime/Renderer/GraphicsContext.h"
#include <memory>

struct GLFWwindow;

namespace Candy {

	class VulkanDevice;
	class VulkanSwapChain;

	// =========================================================================
	// VulkanGraphicsContext — owns VulkanDevice + VulkanSwapChain for main window
	// =========================================================================
	class VulkanGraphicsContext : public GraphicsContext
	{
	public:
		VulkanGraphicsContext(const WindowHandle& handle);
		virtual ~VulkanGraphicsContext();

		void Init() override;
		void SwapBuffers() override;

		[[nodiscard]] VulkanDevice*     GetDevice()     const { return m_Device.get(); }
		[[nodiscard]] VulkanSwapChain*  GetSwapChain()  const { return m_SwapChain.get(); }

	private:
		GLFWwindow*                          m_Window = nullptr;
		std::unique_ptr<VulkanDevice>        m_Device;
		std::unique_ptr<VulkanSwapChain>     m_SwapChain;
	};

} // namespace Candy
