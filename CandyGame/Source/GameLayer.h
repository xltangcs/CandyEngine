#pragma once

#include "Candy.h"
#include "Candy/Scene/Scene.h"
#include "Candy/Scene/SceneSerializer.h"
#include "Candy/Scene/Entity.h"

namespace Candy {

	class GameLayer : public Layer
	{
	public:
		GameLayer();
		virtual ~GameLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		void OnUpdate(Timestep ts) override;
		void OnEvent(Event& e) override;

	private:
		Ref<Scene> m_ActiveScene;
	};

}
