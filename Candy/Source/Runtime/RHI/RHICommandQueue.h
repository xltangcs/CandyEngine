#pragma once

#include "Runtime/Core/Base.h"

#include <vector>

namespace Candy {

	// Forward declarations
	class RHICommandBuffer;
	class RHISwapChain;

	// =========================================================================
	// RHICommandQueue — submits command buffers to the GPU for execution
	// =========================================================================
	class RHICommandQueue
	{
	public:
		virtual ~RHICommandQueue() = default;

		/// Allocates a new command buffer from the queue's internal pool.
		virtual Scope<RHICommandBuffer> CreateCommandBuffer() = 0;

		/// Submits one or more command buffers for execution.
		/// The caller must ensure the command buffers live until the GPU has
		/// finished consuming them (via fences).
		virtual void Submit(const std::vector<RHICommandBuffer*>& commandBuffers) = 0;

		/// Presents the swap-chain back buffer to the screen / window.
		virtual void Present(const Ref<RHISwapChain>& swapChain) = 0;

		/// Blocks the CPU until the queue has drained all submitted work.
		virtual void WaitIdle() = 0;
	};

} // namespace Candy
