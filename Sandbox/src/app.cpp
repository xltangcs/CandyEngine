#include "Candy.h"

class ExampleLayer : public Candy::Layer
{
public:
	ExampleLayer()
		: Layer("Example")
	{
	}

	void OnUpdate() override
	{
		CANDY_INFO("ExampleLayer::Update");
	}

	void OnEvent(Candy::Event& event) override
	{
		CANDY_TRACE("{0}", event);
	}

};

class Sandbox : public Candy::Application
{
public :
	Sandbox()
	{
		PushLayer(new ExampleLayer());
	}

	~Sandbox()
	{

	}

};

Candy::Application* Candy::CreateApplication()
{
	return new Sandbox();
}