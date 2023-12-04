#pragma once

#include "Candy/Layer.h"
#include "Candy/Events/ApplicationEvent.h"
#include "Candy/Events/KeyEvent.h"
#include "Candy/Events/MouseEvent.h"

namespace Candy {

	class CANDY_API ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnImGuiRender() override;

		void Begin();
		void End();
	private:
		float m_Time = 0.0f;
	};

}