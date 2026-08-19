#include "CandyPCH.h"
#include "EditorCamera.h"

#include "Runtime/Core/Input.h"
#include "Runtime/Core/KeyCodes.h"
#include "Runtime/Core/MouseCodes.h"
#include "Runtime/Core/Application.h"

#include <glfw/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Candy {

	EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
		: m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip), Camera(glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip))
	{
		UpdateView();
	}

	void EditorCamera::UpdateProjection()
	{
		m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
		m_Projection = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
	}

	void EditorCamera::UpdateView()
	{
		// m_Yaw = m_Pitch = 0.0f; // Lock the camera's rotation
		m_Position = CalculatePosition();

		glm::quat orientation = GetOrientation();
		m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
		m_ViewMatrix = glm::inverse(m_ViewMatrix);
	}

	void EditorCamera::Focus(const glm::vec3& center, float radius)
	{
		// UE FocusViewportOnBounds style: keep the current yaw/pitch (view
		// direction) and derive the distance from the bounding-sphere radius
		// so the object is fully framed, with some padding.
		float halfExtent = std::max(radius, 0.5f);

		float halfFovV = glm::radians(m_FOV) * 0.5f;
		float halfFovH = std::atan(std::tan(halfFovV) * m_AspectRatio);

		float dist = halfExtent / std::sin(halfFovV);
		dist = std::max(dist, halfExtent / std::sin(halfFovH));

		m_Distance = glm::clamp(dist * 1.15f, 1.0f, m_FarClip * 0.9f);
		m_FocalPoint = center;
		UpdateView();
	}

	void EditorCamera::RestoreView(const glm::vec3& focalPoint, float pitch, float yaw, float distance)
	{
		m_FocalPoint = focalPoint;
		m_Pitch = glm::clamp(pitch, -89.9f, 89.9f);
		m_Yaw = yaw;
		m_Distance = glm::max(distance, 1.0f);
		UpdateView();
	}

	std::pair<float, float> EditorCamera::PanSpeed() const
	{
		float x = std::min(m_ViewportWidth / 1000.0f, 2.4f); // max = 2.4f
		float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

		float y = std::min(m_ViewportHeight / 1000.0f, 2.4f); // max = 2.4f
		float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

		return { xFactor, yFactor };
	}

	float EditorCamera::RotationSpeed() const
	{
		return 0.8f;
	}

	float EditorCamera::ZoomSpeed() const
	{
		float distance = m_Distance * 0.2f;
		distance = std::max(distance, 0.0f);
		float speed = distance * distance;
		speed = std::min(speed, 100.0f); // max speed = 100
		return speed;
	}

	void EditorCamera::OnUpdate(Timestep ts)
	{
		const glm::vec2& mouse{ Input::GetMouseX(), Input::GetMouseY() };
		glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.003f;
		m_InitialMousePosition = mouse;

		if (Input::IsKeyPressed(Key::LeftAlt))
		{
			if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle))
				MousePan(delta);
			else if (Input::IsMouseButtonPressed(Mouse::ButtonLeft))
				MouseRotate(delta);
			else if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
				MouseZoom(delta.y);
		}
		else if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle))
		{
			MousePan(delta);
		}
		else if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
		{
			if (!m_IsFlying)
				EnterFlyMode();
			MouseFly(delta, ts);
		}
		else if (m_IsFlying)
		{
			ExitFlyMode();
		}

		UpdateView();
	}

	void EditorCamera::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>(CANDY_BIND_EVENT_FN(EditorCamera::OnMouseScroll));
	}

	bool EditorCamera::OnMouseScroll(MouseScrolledEvent& e)
	{
		float delta = e.GetYOffset() * 0.1f;
		MouseZoom(delta);
		UpdateView();
		return false;
	}

	void EditorCamera::MousePan(const glm::vec2& delta)
	{
		auto [xSpeed, ySpeed] = PanSpeed();
		m_FocalPoint += -GetRightDirection() * delta.x * xSpeed * m_Distance;
		m_FocalPoint += GetUpDirection() * delta.y * ySpeed * m_Distance;
	}

	void EditorCamera::MouseRotate(const glm::vec2& delta)
	{
		float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
		m_Yaw += yawSign * delta.x * RotationSpeed();
		m_Pitch += delta.y * RotationSpeed();
		m_Pitch = glm::clamp(m_Pitch, -89.9f, 89.9f);
	}

	void EditorCamera::MouseZoom(float delta)
	{
		float newDistance = m_Distance - delta * ZoomSpeed();
		newDistance = std::max(newDistance, 1.0f);

		// Keep the world point under the cursor stationary while zooming
		// (UE-style zoom-to-cursor): move the camera along the cursor ray so
		// that the point under the mouse stays under the mouse after the zoom.
		glm::vec3 mouseWorld = GetMouseWorldPoint();
		glm::vec3 rayDir = glm::normalize(mouseWorld - m_Position);
		m_Position += rayDir * (m_Distance - newDistance);
		m_FocalPoint = m_Position + GetForwardDirection() * newDistance;
		m_Distance = newDistance;
	}

	void EditorCamera::MouseFly(const glm::vec2& delta, Timestep ts)
	{
		float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
		m_Yaw += yawSign * delta.x * RotationSpeed();
		m_Pitch += delta.y * RotationSpeed();
		m_Pitch = glm::clamp(m_Pitch, -89.9f, 89.9f);

		float speed = m_FlySpeed * (m_Distance / 10.0f);
		if (Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift))
			speed *= 3.0f;

		glm::vec3 move(0.0f);
		if (Input::IsKeyPressed(Key::W)) move += GetForwardDirection();
		if (Input::IsKeyPressed(Key::S)) move -= GetForwardDirection();
		if (Input::IsKeyPressed(Key::A)) move -= GetRightDirection();
		if (Input::IsKeyPressed(Key::D)) move += GetRightDirection();
		if (Input::IsKeyPressed(Key::E)) move += GetUpDirection();
		if (Input::IsKeyPressed(Key::Q)) move -= GetUpDirection();

		if (glm::length(move) > 0.0f)
		{
			m_FocalPoint += glm::normalize(move) * speed * ts.GetSeconds();
			m_Position = CalculatePosition();
		}
	}

	void EditorCamera::EnterFlyMode()
	{
		m_IsFlying = true;
		GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}

	void EditorCamera::ExitFlyMode()
	{
		m_IsFlying = false;
		GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}

	glm::vec3 EditorCamera::GetUpDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glm::vec3 EditorCamera::GetRightDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	glm::vec3 EditorCamera::GetForwardDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
	}

	glm::vec3 EditorCamera::CalculatePosition() const
	{
		return m_FocalPoint - GetForwardDirection() * m_Distance;
	}

	glm::vec3 EditorCamera::GetMouseWorldPoint() const
	{
		float viewportW = std::max(m_ViewportWidth, 1.0f);
		float viewportH = std::max(m_ViewportHeight, 1.0f);
		float ndcX = (m_ViewportMousePos.x / viewportW) * 2.0f - 1.0f;
		float ndcY = 1.0f - (m_ViewportMousePos.y / viewportH) * 2.0f;

		glm::vec4 rayClip(ndcX, ndcY, -1.0f, 1.0f);
		glm::vec4 rayEye = glm::inverse(m_Projection) * rayClip;
		rayEye.z = -1.0f;
		rayEye.w = 0.0f;

		glm::vec4 rayWorld4 = glm::inverse(m_ViewMatrix) * rayEye;
		glm::vec3 rayWorld = glm::normalize(glm::vec3(rayWorld4));

		return m_Position + rayWorld * m_Distance;
	}

	glm::quat EditorCamera::GetOrientation() const
	{
		return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
	}

}