#pragma once

#include "Runtime/Renderer/RendererAPI.h"

namespace Candy {
	class OpenGLRendererAPI : public RendererAPI
	{
	public:
		virtual void Init(const PipelineStateDescription& defaultState) override;
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void Clear() override;

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, PrimitiveTopology topology, uint32_t indexCount = 0) override;

		virtual void Draw(const Ref<VertexArray>& vertexArray, PrimitiveTopology topology, uint32_t elementCount) override;

		virtual void SetLineWidth(float width) override;
		virtual void SetDefaultPipelineState(const PipelineStateDescription& state) override;
	};
}

