#include "CandyPCH.h"

#include "Runtime/RHI/RHIContext.h"
#include "Runtime/RHI/RHIDevice.h"
#include "Runtime/RHI/RHISwapChain.h"
#include "Runtime/RHI/RHICommandBuffer.h"
#include "Runtime/RHI/RHICommandQueue.h"

namespace Candy {

	namespace {
		// Process-wide singletons — non-owning, registered by the active
		// GraphicsContext during Init() / per-frame Begin().
		RHIDevice*        g_Device        = nullptr;
		RHISwapChain*     g_SwapChain     = nullptr;
		RHICommandBuffer* g_CommandBuffer = nullptr;
	}

	void RHIContext::SetDevice(RHIDevice* device)                 { g_Device = device; }
	void RHIContext::SetSwapChain(RHISwapChain* swapChain)        { g_SwapChain = swapChain; }
	void RHIContext::SetCurrentCommandBuffer(RHICommandBuffer* cmd) { g_CommandBuffer = cmd; }

	RHIDevice*         RHIContext::GetDevice()                { return g_Device; }
	RHISwapChain*      RHIContext::GetSwapChain()            { return g_SwapChain; }
	RHICommandBuffer*  RHIContext::GetCurrentCommandBuffer() { return g_CommandBuffer; }

	RHICommandQueue* RHIContext::GetCommandQueue()
	{
		return g_Device ? &g_Device->GetCommandQueue() : nullptr;
	}

} // namespace Candy
