#pragma once

#include "Runtime/Renderer/RendererAPI.h"

namespace Candy {
	class RenderCommand
	{
	public:
		static void Init(const PipelineStateDescription& defaultState)
		{
			s_RendererAPI->Init(defaultState);
		}
		static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			s_RendererAPI->SetViewport(x, y, width, height);
		}
		static void SetClearColor(const glm::vec4& color)
		{
			s_RendererAPI->SetClearColor(color);
		}

		static void Clear()
		{
			s_RendererAPI->Clear();
		}

		static void DrawIndexed(const Ref<VertexArray>& vertexArray, PrimitiveTopology topology, uint32_t indexCount = 0)
		{
			s_RendererAPI->DrawIndexed(vertexArray, topology, indexCount);
		}

		static void Draw(const Ref<VertexArray>& vertexArray, PrimitiveTopology topology, uint32_t elementCount)
		{
			s_RendererAPI->Draw(vertexArray, topology, elementCount);
		}

		static void SetLineWidth(float width)
		{
			s_RendererAPI->SetLineWidth(width);
		}
	private:
		static Scope<RendererAPI> s_RendererAPI;
	};
}

