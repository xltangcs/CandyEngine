#pragma once

#include "Runtime/Core/Base.h"

namespace Candy {

	class RHIDevice;
	class RHICommandQueue;
	class RHISwapChain;
	class RHICommandBuffer;

	// =========================================================================
	// RHIContext — process-wide singletons for the active RHI backend
	//
	// Backends (D3D12GraphicsContext / OpenGLRHIDevice / VulkanGraphicsContext)
	// publish their device / swap chain / per-frame command buffer here during
	// initialization and frame setup. Renderer code reads from this registry
	// instead of including Platform/* headers.
	//
	// All pointers are non-owning; lifetime is managed by the GraphicsContext
	// that published them.
	// =========================================================================
	class RHIContext
	{
	public:
		// ---- Publisher API (called by backend GraphicsContext) ------------
		static void SetDevice(RHIDevice* device);
		static void SetSwapChain(RHISwapChain* swapChain);
		static void SetCurrentCommandBuffer(RHICommandBuffer* cmd);

		// ---- Reader API (called by Renderer2D / Editor / etc.) ------------
		[[nodiscard]] static RHIDevice*        GetDevice();
		[[nodiscard]] static RHISwapChain*     GetSwapChain();
		[[nodiscard]] static RHICommandBuffer* GetCurrentCommandBuffer();
		[[nodiscard]] static RHICommandQueue*  GetCommandQueue();

	private:
		RHIContext() = delete;
	};

} // namespace Candy
