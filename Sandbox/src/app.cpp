#include "Candy.h"


class Sandbox : public Candy::Application
{
public :
	Sandbox()
	{

	}

	~Sandbox()
	{

	}

};

Candy::Application* Candy::CreateApplication()
{
	return new Sandbox();
}