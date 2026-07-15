#include "Candy.h"

#include "Candy/Core/EntryPoint.h"

#include "Sandbox2D.h"
#include "EXampleLayer.h"

class Sandbox : public Candy::Application
{
public :
	Sandbox()
	{
		//PushLayer(new ExampleLayer());
		PushLayer(new Sandbox2D());
	}

	~Sandbox()
	{

	}

};

Candy::Application* Candy::CreateApplication(int argc, char** argv)
{
	return new Sandbox();
}