#pragma once

#include "candy.h"

class ExampleLayer : public Candy::Layer
{
public:
	ExampleLayer();
	virtual ~ExampleLayer() = default;

	virtual void OnAttach() override;
	virtual void OnDetach() override;

	void OnUpdate(Candy::Timestep ts) override;
	virtual void OnImGuiRender() override;
	void OnEvent(Candy::Event& e) override;
private:
	Candy::ShaderLibrary m_ShaderLibrary;
	Candy::Ref<Candy::Shader> m_Shader;
	Candy::Ref<Candy::VertexArray> m_VertexArray;

	Candy::Ref<Candy::Shader> m_FlatColorShader;
	Candy::Ref<Candy::VertexArray> m_SquareVA;

	Candy::Ref<Candy::Texture2D> m_Texture, m_CandyLogTexture;

	Candy::OrthographicCameraController m_CameraController;
	glm::vec3 m_SquareColor = { 0.2f, 0.3f, 0.8f };
};