#pragma once

namespace Candy {

	// =========================================================================
	// Pipeline state description - platform-agnostic, replaces OpenGL global
	// state machine (glEnable/glDisable)
	// =========================================================================
	struct PipelineStateDescription
	{
		bool Blend = true;
		bool DepthTest = true;
		bool LineSmooth = true;
		// Extensible: CullMode, FillMode, ColorWriteMask, StencilState, etc.
	};

} // namespace Candy
