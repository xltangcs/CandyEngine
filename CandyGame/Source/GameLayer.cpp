#include "GameLayer.h"

#include <algorithm>

#include "Runtime/Renderer/Framebuffer.h"
#include "Runtime/Renderer/GameFrameRenderer.h"
#include "Runtime/Project/Project.h"
#include "Runtime/Core/FileSystem.h"
#include "Runtime/Asset/MeshImporter.h"
#include "Runtime/Asset/StaticMeshResource.h"
#include "Runtime/Asset/Material.h"

#include <GLFW/glfw3.h>
#include <imgui/imgui.h>

namespace Candy {

	GameLayer::GameLayer()
		: Layer("GameLayer")
	{
	}

	void GameLayer::OnAttach()
	{
#ifdef CANDY_DEBUG
		// TEMP: Step 3 — MeshImporter smoke test on the lemon model.
		{
			const std::string testModel = "VFS://Game/Model/lemon/lemon_1k.gltf";
			auto imported = MeshImporter::ImportStaticMesh(testModel);
			if (imported && imported->Mesh)
			{
				CANDY_CORE_INFO("==== MeshImporter test OK ====");
				CANDY_CORE_INFO("  vertices : {}", imported->Mesh->Vertices.size());
				CANDY_CORE_INFO("  indices  : {}", imported->Mesh->Indices.size());
				CANDY_CORE_INFO("  submeshes: {}", imported->Mesh->Submeshes.size());
				for (const auto& sub : imported->Mesh->Submeshes)
					CANDY_CORE_INFO("    submesh '{}' offset={} count={} matIdx={} matName='{}'",
						sub.Name, sub.IndexOffset, sub.IndexCount, sub.MaterialIndex, sub.MaterialName);
				CANDY_CORE_INFO("  materials: {}", imported->Materials.size());
				for (const auto& mat : imported->Materials)
				{
					CANDY_CORE_INFO("    material '{}' params:", mat->Name);
					for (const auto& [key, value] : mat->ShaderParams)
						CANDY_CORE_INFO("      {} (type index {})", key, value.index());
				}
				CANDY_CORE_INFO("  bounds  : center=({},{},{}) extent=({},{},{})",
					imported->Mesh->Bounds.GetCenter().x, imported->Mesh->Bounds.GetCenter().y, imported->Mesh->Bounds.GetCenter().z,
					imported->Mesh->Bounds.GetExtent().x, imported->Mesh->Bounds.GetExtent().y, imported->Mesh->Bounds.GetExtent().z);
			}
			else
			{
				CANDY_CORE_ERROR("==== MeshImporter test FAILED ====");
			}
		}
#endif

		auto project = Application::Get().GetProject();
		if (!project)
		{
			CANDY_CORE_ERROR("No project loaded");
			return;
		}

		m_ActiveScene = CreateRef<Scene>();

		auto sceneName = project->GetDefaultScene();
		if (sceneName.empty())
		{
			CANDY_CORE_ERROR("No DefaultScene set in project");
			return;
		}

		// DefaultScene is stored in VFS:// format.
		auto vfsScenePath = sceneName;
		if (FileSystem::Get().Exists(vfsScenePath))
		{
			auto yamlContent = FileSystem::Get().ReadText(vfsScenePath);
			if (yamlContent)
			{
				SceneSerializer serializer(m_ActiveScene);
				if (serializer.DeserializeFromString(*yamlContent))
				{
					CANDY_CORE_INFO("Loaded scene from VFS: {0}", vfsScenePath);
					auto& window = Application::Get().GetWindow();
					auto w = window.GetWidth();
					auto h = window.GetHeight();
					m_ActiveScene->OnViewportResize(w, h);

					// Create SwapChainTarget framebuffer (binds to framebuffer 0)
					FramebufferDesc fbSpec;
					fbSpec.Width = w;
					fbSpec.Height = h;
					fbSpec.SwapChainTarget = true;
					m_GameFramebuffer = Framebuffer::Create(fbSpec);

					m_ActiveScene->OnRuntimeStart();
					return;
				}
			}
		}

		CANDY_CORE_ERROR("Scene not found in VFS: {0}", vfsScenePath);
	}

	void GameLayer::OnDetach()
	{
		if (m_ActiveScene)
			m_ActiveScene->OnRuntimeStop();
	}

	void GameLayer::OnUpdate(Timestep ts)
	{
		if (!m_ActiveScene || !m_GameFramebuffer)
			return;

		// Resize framebuffer if window changed
		auto& window = Application::Get().GetWindow();
		uint32_t w = window.GetWidth();
		uint32_t h = window.GetHeight();
		if (m_GameFramebuffer->GetWidth() != w || m_GameFramebuffer->GetHeight() != h)
			m_GameFramebuffer->Resize(w, h);

		// Update logic (physics, scripts, audio)
		m_ActiveScene->OnUpdateRuntimeLogic(ts);

		// Render scene + UI into SwapChainTarget (framebuffer 0).
		// Clear/viewport semantics live in SceneRenderer::EndFrame (LoadOp::Clear
		// on the first pass after SetActiveRenderTarget; swap-chain path clears).
		m_GameFramebuffer->Bind();
		GameFrameRenderer::RenderSceneTo(*m_GameFramebuffer, *m_ActiveScene, nullptr, ts.GetSeconds());

		// Mouse position in window coordinates (fullscreen game = 1:1)
		float mouseX = (float)Input::GetMouseX();
		float mouseY = (float)Input::GetMouseY();
		bool mouseDown = Input::IsMouseButtonPressed(Mouse::ButtonLeft);

		GameFrameRenderer::RenderUITo(*m_GameFramebuffer, *m_ActiveScene, mouseX, mouseY, mouseDown, ts.GetSeconds());

		m_GameFramebuffer->Unbind();
	}

	void GameLayer::OnEvent(Event& e)
	{
		// Handle window resize for SwapChainTarget framebuffer
		if (e.GetEventType() == EventType::WindowResize)
		{
			auto& re = (WindowResizeEvent&)e;
			if (m_GameFramebuffer)
				m_GameFramebuffer->Resize(re.GetWidth(), re.GetHeight());
			// Update scene cameras' aspect ratio -- without this, the scene renders with
			// the initial aspect and gets stretched when the window is resized.
			if (m_ActiveScene)
				m_ActiveScene->OnViewportResize(re.GetWidth(), re.GetHeight());
		}
	}

}
