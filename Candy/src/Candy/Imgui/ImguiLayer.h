#pragma once

#include "Candy/Core/Layer.h"
#include "Candy/Events/ApplicationEvent.h"
#include "Candy/Events/KeyEvent.h"
#include "Candy/Events/MouseEvent.h"

namespace Candy {

	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnEvent(Event& e) override;

		void Begin();
		void End();
		void BlockEvents(bool block) { m_BlockEvents = block; }
	private:
		bool m_BlockEvents = true;
		float m_Time = 0.0f;
	};

}