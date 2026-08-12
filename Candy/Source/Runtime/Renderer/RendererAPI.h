#pragma once

#include <string>

namespace Candy {

	// =========================================================================
	// RendererAPI — global backend selection holder (no backend objects)
	//
	// This used to be a virtual interface (Init/SetViewport/Clear/Draw...) with
	// per-backend implementations, accessed through RenderCommand. That dual
	// abstraction leaked responsibilities (e.g. D3D12 Clear() was a no-op for
	// framebuffers, forcing clear semantics into Renderer2D's render passes).
	// It is now retired: rendering goes through the RHI layer, and this class
	// only keeps the process-wide "which backend is active" selector used by
	// the legacy factories (Buffer/Texture/Shader/VertexArray/Framebuffer/
	// GraphicsContext) and by API-conditional editor code.
	// =========================================================================
	class RendererAPI
	{
	public:
		enum class API
		{
			None = 0, OpenGL = 1, Vulkan = 2, D3D12 = 3
		};

		inline static API GetAPI() { return s_API; }

		/// Runtime setter — only safe to call BEFORE Window/GraphicsContext
		/// have been initialised. Used by project-load recipes that pick the
		/// backend from the .candyproj file at startup.
		static void SetAPI(API api) { s_API = api; }

		/// String ↔ API mapping so the active backend can be persisted as a
		/// human-readable string inside the project file.
		static API         APIFromString(const std::string& str);
		static const char* StringFromAPI(API api);

	private:
		static API s_API;
	};
}
