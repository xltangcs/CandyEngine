#include "Sandbox2D.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

Sandbox2D::Sandbox2D()
	: Layer("Sandbox2D"), m_CameraController(1280.0f / 720.0f)
{
}

void Sandbox2D::OnAttach()
{
	m_CheckerboardTexture = Candy::Texture2D::Create("assets/textures/Checkerboard.png");
}

void Sandbox2D::OnDetach()
{
	
}

void Sandbox2D::OnUpdate(Candy::Timestep ts)
{
	// Update
	m_CameraController.OnUpdate(ts);

	// Render
	Candy::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	Candy::RenderCommand::Clear();

	Candy::Renderer2D::BeginScene(m_CameraController.GetCamera());

	Candy::Renderer2D::DrawQuad({ -1.0f, 0.0f }, { 0.8f, 0.8f }, { 0.8f, 0.2f, 0.3f, 1.0f });
	Candy::Renderer2D::DrawQuad({ 0.5f, -0.5f }, { 0.5f, 0.75f }, { 0.2f, 0.3f, 0.8f, 1.0f });
	Candy::Renderer2D::DrawQuad({ 0.0f, 0.0f, -0.1f }, { 10.0f, 10.0f }, m_CheckerboardTexture);
	Candy::Renderer2D::DrawRotatedQuad({ 0.25f, 0.25f }, { 1.0f, 1.0f }, 45, { 0.8f, 0.2f, 0.2f, 1.0f });

	Candy::Renderer2D::EndScene();
}

void Sandbox2D::OnImGuiRender()
{
	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_SquareColor));
	ImGui::End();
}

void Sandbox2D::OnEvent(Candy::Event& e)
{
	m_CameraController.OnEvent(e);
}