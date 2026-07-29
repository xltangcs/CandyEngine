#include "CandyPCH.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

#include <glad/glad.h>

namespace Candy {

	static GLenum TopologyToGL(PrimitiveTopology topology)
	{
		switch (topology)
		{
		case PrimitiveTopology::Triangles: return GL_TRIANGLES;
		case PrimitiveTopology::Lines:     return GL_LINES;
		case PrimitiveTopology::Points:    return GL_POINTS;
		default:
			CANDY_CORE_ASSERT(false, "Unknown PrimitiveTopology!");
			return GL_TRIANGLES;
		}
	}

	void OpenGLMessageCallback(
		unsigned source,
		unsigned type,
		unsigned id,
		unsigned severity,
		int length,
		const char* message,
		const void* userParam)
		{
			switch (severity)
			{
			case GL_DEBUG_SEVERITY_HIGH:         CANDY_CORE_CRITICAL(message); return;
			case GL_DEBUG_SEVERITY_MEDIUM:       CANDY_CORE_ERROR(message); return;
			case GL_DEBUG_SEVERITY_LOW:          CANDY_CORE_WARN(message); return;
			case GL_DEBUG_SEVERITY_NOTIFICATION: CANDY_CORE_TRACE(message); return;
			}

			CANDY_CORE_ASSERT(false, "Unknown severity level!");
		}

	void OpenGLRendererAPI::Init(const PipelineStateDescription& defaultState)
	{
		#ifdef CANDY_DEBUG
				glEnable(GL_DEBUG_OUTPUT);
				glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
				glDebugMessageCallback(OpenGLMessageCallback, nullptr);

				glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
		#endif

		SetDefaultPipelineState(defaultState);
	}

	void OpenGLRendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		glViewport(x, y, width, height);
	}

	void OpenGLRendererAPI::SetClearColor(const glm::vec4& color)
	{
		glClearColor(color.r, color.g, color.b, color.a);
	}

	void OpenGLRendererAPI::Clear()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void OpenGLRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray, PrimitiveTopology topology, uint32_t indexCount)
	{
		vertexArray->Bind();
		uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
		glDrawElements(TopologyToGL(topology), count, GL_UNSIGNED_INT, nullptr);
	}

	void OpenGLRendererAPI::Draw(const Ref<VertexArray>& vertexArray, PrimitiveTopology topology, uint32_t elementCount)
	{
		vertexArray->Bind();
		glDrawArrays(TopologyToGL(topology), 0, elementCount);
	}

	void OpenGLRendererAPI::SetLineWidth(float width)
	{
		glLineWidth(width);
	}

	void OpenGLRendererAPI::SetDefaultPipelineState(const PipelineStateDescription& state)
	{
		if (state.Blend)
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		else
		{
			glDisable(GL_BLEND);
		}

		if (state.DepthTest)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);

		if (state.LineSmooth)
			glEnable(GL_LINE_SMOOTH);
		else
			glDisable(GL_LINE_SMOOTH);
	}

}