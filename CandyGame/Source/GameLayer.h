#pragma once

#include "Candy.h"
#include "Runtime/Scene/Scene.h"
#include "Runtime/Scene/SceneSerializer.h"
#include "Runtime/Scene/Entity.h"

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
		Ref<Framebuffer> m_GameFramebuffer;
	};

}
