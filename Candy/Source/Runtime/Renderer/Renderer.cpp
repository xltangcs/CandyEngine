#include "CandyPCH.h"

#include "Runtime/Renderer/Renderer.h"
#include "Runtime/Renderer/Renderer2D.h"

#include "Runtime/RHI/RHIPipelineState.h"

namespace Candy {
	Scope<Renderer::SceneData> Renderer::s_SceneData = CreateScope<Renderer::SceneData>();

	void Renderer::Init()
	{
		PipelineStateDescription defaultState;
		defaultState.Blend = true;
		defaultState.DepthTest = true;
		defaultState.LineSmooth = true;

		RenderCommand::Init(defaultState);
		Renderer2D::Init();
	}

	void Renderer::Shutdown()
	{
		Renderer2D::Shutdown();
	}

	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		RenderCommand::SetViewport(0, 0, width, height);
	}

	void Renderer::BeginScene(OrthographicCamera& camera)
	{
		s_SceneData->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
	}

	void Renderer::EndScene()
	{

	}
	void Renderer::Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform)
	{
		shader->Bind();
		shader->SetMat4("u_ViewProjection", s_SceneData->ViewProjectionMatrix);
		shader->SetMat4("u_Transform", transform);

		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray, PrimitiveTopology::Triangles);
	}
}
