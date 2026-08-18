#pragma once

#include "Camera.h"
#include "Runtime/Core/Timestep.h"
#include "Runtime/Events/Event.h"
#include "Runtime/Events/MouseEvent.h"

#include <glm/glm.hpp>

namespace Candy {

	class EditorCamera : public Camera
	{
	public:
		EditorCamera() = default;
		EditorCamera(float fov, float aspectRatio, float nearClip, float farClip);

		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);

		// UE-style fit-bounds focus: keeps the current view direction and
		// moves the camera so the sphere (center, radius) is fully in view.
		void Focus(const glm::vec3& center, float radius);

		inline float GetDistance() const { return m_Distance; }
		inline void SetDistance(float distance) { m_Distance = distance; }

		inline void SetViewportSize(float width, float height) { m_ViewportWidth = width; m_ViewportHeight = height; UpdateProjection(); }
		inline void SetViewportMousePosition(float x, float y) { m_ViewportMousePos = { x, y }; }
		inline void SetFocalPoint(const glm::vec3& focalPoint) { m_FocalPoint = focalPoint; }
		inline bool IsFlying() const { return m_IsFlying; }

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		glm::mat4 GetViewProjection() const { return m_Projection * m_ViewMatrix; }

		glm::vec3 GetUpDirection() const;
		glm::vec3 GetRightDirection() const;
		glm::vec3 GetForwardDirection() const;
		const glm::vec3& GetPosition() const { return m_Position; }
		glm::quat GetOrientation() const;

		float GetPitch() const { return m_Pitch; }
		float GetYaw() const { return m_Yaw; }
	private:
		void UpdateProjection();
		void UpdateView();

		bool OnMouseScroll(MouseScrolledEvent& e);

		void MousePan(const glm::vec2& delta);
		void MouseRotate(const glm::vec2& delta);
		void MouseZoom(float delta);
		void MouseFly(const glm::vec2& delta, Timestep ts);

		void EnterFlyMode();
		void ExitFlyMode();

		glm::vec3 CalculatePosition() const;
		glm::vec3 GetMouseWorldPoint() const;

		std::pair<float, float> PanSpeed() const;
		float RotationSpeed() const;
		float ZoomSpeed() const;
	private:
		float m_FOV = 45.0f, m_AspectRatio = 1.778f, m_NearClip = 0.1f, m_FarClip = 1000.0f;

		glm::mat4 m_ViewMatrix;
		glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
		glm::vec3 m_FocalPoint = { 0.0f, 0.0f, 0.0f };

		glm::vec2 m_InitialMousePosition = { 0.0f, 0.0f };

		float m_Distance = 10.0f;
		float m_Pitch = 0.0f, m_Yaw = 0.0f;

		float m_ViewportWidth = 1280, m_ViewportHeight = 720;
		glm::vec2 m_ViewportMousePos = { 0.0f, 0.0f };

		bool m_IsFlying = false;
		float m_FlySpeed = 10.0f;
	};

}