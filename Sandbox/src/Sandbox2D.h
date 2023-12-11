#pragma once

#include "Candy.h"

class Sandbox2D : public Candy::Layer
{
public:
	Sandbox2D();
	virtual ~Sandbox2D() = default;

	virtual void OnAttach() override;
	virtual void OnDetach() override;

	void OnUpdate(Candy::Timestep ts) override;
	virtual void OnImGuiRender() override;
	void OnEvent(Candy::Event& e) override;
private:
	Candy::OrthographicCameraController m_CameraController;

	// Temp
	Candy::Ref<Candy::VertexArray> m_SquareVA;
	Candy::Ref<Candy::Shader> m_FlatColorShader;
	Candy::Ref<Candy::Framebuffer> m_Framebuffer;

	Candy::Ref<Candy::Texture2D> m_CheckerboardTexture;

	glm::vec4 m_SquareColor = { 0.2f, 0.3f, 0.8f, 1.0f };
};
