#pragma once

#include "Runtime/RHI/RHITypes.h"
#include "Runtime/RHI/RHIRenderPass.h"
#include "Runtime/Core/Base.h"

#include <string>

// Forward declarations (Candy namespace)
namespace Candy {
	class RHIGraphicsPipeline;
} // namespace Candy

namespace Candy::IR {

	// =========================================================================
	// IRCommandValidator — validates command-buffer recording state at
	// debug time.  Catches common errors such as:
	//   - Draw() called before BeginRenderPass()
	//   - BeginRenderPass() called while already inside a pass
	//   - Draw() without SetPipeline()
	//
	// In release builds (defined CANDY_RELEASE / CANDY_DIST) all checks
	// are compiled out via #ifdef for zero overhead.
	// =========================================================================
	class IRCommandValidator
	{
	public:
		IRCommandValidator() = default;

		// ---- State transitions (called by IR command buffer wrappers) ------

		void OnBegin();
		void OnEnd();

		void OnBeginRenderPass(const Candy::RenderPassDesc& desc);
		void OnEndRenderPass();

		void OnSetPipeline(const Candy::Ref<Candy::RHIGraphicsPipeline>& pipeline);
		void OnSetVertexBuffer(uint32_t slot);
		void OnSetIndexBuffer();

		void OnDraw(uint32_t vertexCount);
		void OnDrawIndexed(uint32_t indexCount);

		// ---- Queries -------------------------------------------------------

		[[nodiscard]] bool IsRecording()    const { return m_Recording; }
		[[nodiscard]] bool IsInRenderPass() const { return m_InRenderPass; }

	private:
		bool m_Recording         = false;
		bool m_InRenderPass      = false;
		bool m_PipelineSet       = false;
		bool m_VertexBufferBound = false;
		bool m_IndexBufferBound  = false;
	};

} // namespace Candy::IR
