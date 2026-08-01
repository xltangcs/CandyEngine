#pragma once

#include <glm/glm.hpp>

#include "Runtime/Renderer/VertexArray.h"
#include "Runtime/RHI/RHITypes.h"
#include "Runtime/RHI/RHIPipelineState.h"

namespace Candy {
	class RendererAPI
	{
	public:
		enum class API
		{
			None = 0, OpenGL = 1, Vulkan = 2, D3D12 = 3
		};
	public:
		virtual ~RendererAPI() = default;
		virtual void Init(const PipelineStateDescription& defaultState) = 0;
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
		virtual void SetClearColor(const glm::vec4& color) = 0;
		virtual void Clear() = 0;

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, PrimitiveTopology topology, uint32_t indexCount = 0) = 0;
		virtual void Draw(const Ref<VertexArray>& vertexArray, PrimitiveTopology topology, uint32_t elementCount) = 0;

		virtual void SetLineWidth(float width) = 0;
		virtual void SetDefaultPipelineState(const PipelineStateDescription& state) = 0;

		inline static API GetAPI() { return s_API; }

		/// Runtime setter — only safe to call BEFORE Window/GraphicsContext
		/// have been initialised. Used by project-load recipes that pick the
		/// backend from the .candyproj file at startup.
		static void SetAPI(API api) { s_API = api; }

		/// String ↔ API mapping so the active backend can be persisted as a
		/// human-readable string inside the project file.
		static API         APIFromString(const std::string& str);
		static const char* StringFromAPI(API api);

		static Scope<RendererAPI> Create();

	private:
		static API s_API;
	};
}

