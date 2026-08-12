#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/RHI/RHITypes.h"

#include <cstdint>
#include <string>

namespace Candy {

	// Forward declarations
	class RHITexture;

	// =========================================================================
	// SwapChainDesc
	// =========================================================================
	struct SwapChainDesc
	{
		WindowHandle Window;
		uint32_t     Width          = 0;
		uint32_t     Height         = 0;
		RHIFormat    Format         = RHIFormat::B8G8R8A8Unorm;
		uint32_t     BufferCount    = 2;   ///< double-buffering
		bool         VSync          = true;
	};

	// =========================================================================
	// RHISwapChain — manages the swap chain and presentable images
	// =========================================================================
	class RHISwapChain
	{
	public:
		virtual ~RHISwapChain() = default;

		virtual const SwapChainDesc& GetDesc() const = 0;

		/// Acquires the next back-buffer index to render into.
		/// Returns the texture that can be used as the current render target.
		virtual Ref<RHITexture> GetCurrentBackBuffer() = 0;

		/// Must be called when the window is resized.  All previously acquired
		/// back-buffer references become invalid.
		virtual void Resize(uint32_t width, uint32_t height) = 0;

		virtual uint32_t GetWidth()  const = 0;
		virtual uint32_t GetHeight() const = 0;
	};

} // namespace Candy
