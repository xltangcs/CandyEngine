#include "CandyPCH.h"

#include "Runtime/Renderer/GameFrameRenderer.h"
#include "Runtime/Core/Application.h"
#include "Runtime/Core/FileSystem.h"
#include "Runtime/Renderer/Framebuffer.h"
#include "Runtime/Renderer/SceneRenderer.h"
#include "Runtime/Renderer/EditorCamera.h"
#include "Runtime/Scene/Scene.h"
#include "Runtime/Scene/Components.h"
#include "Runtime/Imgui/ImguiLayer.h"
#include "Runtime/UI/UISystem.h"
#include "Runtime/RHI/RHICommandQueue.h"
#include "Runtime/RHI/RHIContext.h"

namespace Candy {

	namespace {

		// ---- HDR pipeline state (owned here; editor + game share it) -------

		Ref<Framebuffer> s_HDRSceneTarget;   // float16 + entity-id + depth
		Ref<Framebuffer> s_HDRPreviewTarget; // float16 + depth (no picking)
		Ref<RHIGraphicsPipeline> s_TonemapPipeline;
		Ref<RHIBuffer> s_TonemapCB;

		constexpr const char* kTonemapShaderPath = "VFS://Engine/Content/Shaders/D3D12/Tonemap.hlsl";

		// TonemapCB (b0) — byte-for-byte with Tonemap.hlsl.
		struct TonemapUniforms
		{
			float    Exposure = 1.0f;
			float    _Pad[3]  = { 0.0f, 0.0f, 0.0f };
		};

		// (Re)create the HDR scene/preview target when missing or resized.
		Ref<Framebuffer> EnsureHDRTarget(Ref<Framebuffer>& cached, uint32_t w, uint32_t h, bool withEntityID)
		{
			auto* dev = RHIContext::GetDevice();
			if (!dev)
				return nullptr;

			if (cached && cached->GetWidth() == w && cached->GetHeight() == h)
				return cached;

			FramebufferDesc spec;
			spec.Width  = w;
			spec.Height = h;
			spec.ColorAttachments = { { RHIFormat::R16G16B16A16Float, false } };
			if (withEntityID)
				spec.ColorAttachments.push_back({ RHIFormat::R32Sint, true });
			spec.HasDepthStencil = true;
			spec.DepthStencilAttachment.Format = RHIFormat::D24UnormS8Uint;
			cached = Framebuffer::Create(spec);
			return cached;
		}

		bool EnsureTonemapResources()
		{
			auto* dev = RHIContext::GetDevice();
			if (!dev)
				return false;

			if (!s_TonemapCB)
			{
				BufferDesc cb;
				cb.Size          = 256;
				cb.Usage         = ResourceUsage::ConstantBuffer;
				cb.CPUAccessible = true;
				cb.DebugName     = "GameFrameRenderer_TonemapCB";
				s_TonemapCB = dev->CreateBuffer(cb);
				if (!s_TonemapCB)
					return false;
			}

			if (!s_TonemapPipeline)
			{
				auto src = FileSystem::Get().ReadText(kTonemapShaderPath);
				if (!src)
				{
					CANDY_CORE_ERROR("GameFrameRenderer: failed to load shader '{}'", kTonemapShaderPath);
					return false;
				}

				auto vs = dev->CreateShaderModuleFromSource(src->c_str(), ShaderStage::Vertex,   "VSMain", "Tonemap_VS");
				auto ps = dev->CreateShaderModuleFromSource(src->c_str(), ShaderStage::Fragment, "PSMain", "Tonemap_PS");
				if (!vs || !ps)
				{
					CANDY_CORE_ERROR("GameFrameRenderer: failed to compile Tonemap.hlsl");
					return false;
				}

				GraphicsPipelineDesc td;
				td.Topology            = PrimitiveTopology::Triangles;
				td.Rasterizer.Cull     = CullMode::None;
				td.Rasterizer.Fill     = FillMode::Solid;
				td.Rasterizer.FrontCounterClockwise = true;
				// Depth untouched (Unknown): works for the editor viewport
				// (has DSV) and the game swap chain (no DSV) alike.
				td.DepthStencil.DepthTestEnable  = false;
				td.DepthStencil.DepthWriteEnable = false;
				td.DepthStencil.DepthCompareOp   = CompareOp::Less;
				td.DepthStencilFormat = RHIFormat::Unknown;
				td.Blend.BlendEnable  = false;
				td.RenderTargetFormats = { RHIFormat::R8G8B8A8Unorm };
				// No vertex input: fullscreen triangle driven by SV_VertexID.

				s_TonemapPipeline = dev->CreateGraphicsPipeline(td, vs, ps);
				if (!s_TonemapPipeline)
				{
					CANDY_CORE_ERROR("GameFrameRenderer: failed to create tonemap pipeline");
					return false;
				}
			}

			return true;
		}

		// Fullscreen triangle: sample the HDR source, exposure + ACES filmic +
		// sRGB encode, write into the LDR display target (also the swap chain
		// wrapper in the standalone game). Exposure comes from the frame's
		// skybox component (SkyboxComponent::Exposure = global scene exposure).
		void TonemapPass(Framebuffer* dst, const Ref<Framebuffer>& hdrSrc)
		{
			auto* dev = RHIContext::GetDevice();
			if (!dev || !dst || !hdrSrc || !s_TonemapPipeline || !s_TonemapCB)
				return;

			TonemapUniforms cb;
			cb.Exposure = SceneRenderer::GetSkyboxExposure();
			if (!s_TonemapCB->Write(&cb, sizeof(cb)))
				return;

			Ref<RHITexture> src = hdrSrc->GetColorAttachmentTexture(0);
			if (!src)
			{
				CANDY_CORE_ERROR("GameFrameRenderer::TonemapPass — HDR source has no sampled texture");
				return;
			}

			auto& queue = dev->GetCommandQueue();
			auto cmd = queue.CreateCommandBuffer();
			if (!cmd)
				return;

			cmd->Begin();

			RenderPassDesc rpDesc;
			RenderPassColorAttachment color;
			color.Format = RHIFormat::R8G8B8A8Unorm;
			color.LoadOp = LoadOp::Clear;
			color.ClearColor[0] = 0.0f;
			color.ClearColor[1] = 0.0f;
			color.ClearColor[2] = 0.0f;
			color.ClearColor[3] = 1.0f;
			rpDesc.ColorAttachments.push_back(color);
			cmd->BeginRenderPass(dst, rpDesc);

			cmd->SetViewport(0, 0, static_cast<float>(dst->GetWidth()), static_cast<float>(dst->GetHeight()));
			cmd->SetScissor(0, 0, dst->GetWidth(), dst->GetHeight());

			cmd->SetPipeline(s_TonemapPipeline);
			cmd->SetConstantBuffer(0, 0, s_TonemapCB);
			cmd->SetTextures(2, 1, &src); // root param 2 = SRV table t0
			cmd->Draw(3);

			cmd->EndRenderPass();
			cmd->End();
			queue.Submit({ cmd.get() });
			dev->WaitIdle();
		}

	} // namespace

	void GameFrameRenderer::RenderEditorFrame(const EditorRenderContext& ctx)
	{
		Ref<Framebuffer> hdrScene = EnsureHDRTarget(s_HDRSceneTarget,
			ctx.ViewportTarget->GetWidth(), ctx.ViewportTarget->GetHeight(), true);
		if (!hdrScene || !EnsureTonemapResources())
			return;

		hdrScene->Bind();
		SceneRenderer::SetActiveRenderTarget(hdrScene);
		if (hdrScene->GetColorAttachmentCount() > 1)
			hdrScene->ClearAttachment(1, -1);

		// ---- Scene pass (collect: meshes + sprites/circles) ---------------
		if (ctx.EditorCamera)
			ctx.ActiveScene->RenderScene(*ctx.EditorCamera);
		else
			ctx.ActiveScene->RenderRuntimeScene();

		// ---- Overlay pass (physics colliders etc., debug lines) -----------
		RenderOverlay(ctx);

		// ---- Unified frame submit (scene + overlay lines, one render pass) --
		SceneRenderer::EndFrame();

		// ---- HDR scene → LDR display target (ACES + sRGB) -----------------
		TonemapPass(ctx.ViewportTarget.get(), s_HDRSceneTarget);

		// ---- Camera preview PIP -------------------------------------------
		if (ctx.PreviewTarget && ctx.PreviewEntity)
			RenderCameraPreview(ctx);

		// ---- Game UI composite --------------------------------------------
		// Only while the game is actually running (Play/Simulate). In Edit
		// mode there is no game HUD to show, and rendering it anyway made the
		// game UI present the swap chain an extra time every frame
		// (double-present → startup flicker + swap chain corruption).
		// Composites after the tonemap pass so UI colors are not tonemapped.
		if (ctx.RenderGameUI)
			RenderUITo(*ctx.ViewportTarget, *ctx.ActiveScene, ctx.UIMouseX, ctx.UIMouseY, ctx.UIMouseDown, ctx.DeltaTime);

		ctx.ViewportTarget->Unbind();
	}

	void GameFrameRenderer::RenderSceneTo(Framebuffer& target, Scene& scene, EditorCamera* editorCamera)
	{
		Ref<Framebuffer> hdrScene = EnsureHDRTarget(s_HDRSceneTarget,
			target.GetWidth(), target.GetHeight(), true);
		if (!hdrScene || !EnsureTonemapResources())
			return;

		hdrScene->Bind();
		SceneRenderer::SetActiveRenderTarget(hdrScene);

		if (editorCamera)
		{
			scene.RenderScene(*editorCamera);
		}
		else
		{
			scene.RenderRuntimeScene();
		}
		SceneRenderer::EndFrame();

		// HDR → LDR tonemap into the caller's target (the swap chain wrapper
		// in the standalone game; the LDR viewport framebuffer in the editor).
		TonemapPass(&target, s_HDRSceneTarget);
		// Note: caller is responsible for unbinding (to allow interleaving ReadPixel/OnOverlayRender)
	}

	Ref<Framebuffer> GameFrameRenderer::GetSceneColorTarget()
	{
		return s_HDRSceneTarget;
	}

	void GameFrameRenderer::RenderOverlay(const EditorRenderContext& ctx)
	{
		// Debug line submissions land in the frame's render pass (SceneRenderer
		// draws them after the transparent pass; camera comes from the frame's
		// BeginFrame, so no camera setup is needed here).

		// Skeleton debug lines (skeletal mesh joint hierarchy). Uses the frame's
		// DebugGlobalPose (filled by SkeletalAnimationSystem) transformed by the
		// entity's world transform. Not gated by ShowPhysicsColliders.
		{
			auto view = ctx.ActiveScene->GetAllEntitiesWith<TransformComponent, SkeletalMeshComponent>();
			for (auto entity : view)
			{
				auto [tc, skmc] = view.get<TransformComponent, SkeletalMeshComponent>(entity);
				if (!skmc.ShowSkeleton || !skmc.Mesh || skmc.DebugGlobalPose.size() != skmc.Mesh->Skeleton.size())
					continue;

				const glm::mat4 transform = tc.GetTransform();
				const glm::vec4 color(1.0f, 1.0f, 0.0f, 1.0f);
				for (size_t j = 0; j < skmc.Mesh->Skeleton.size(); j++)
				{
					const int32_t parent = skmc.Mesh->Skeleton[j].ParentIndex;
					if (parent < 0)
						continue;
					const glm::vec3 p0 = glm::vec3(transform * glm::vec4(glm::vec3(skmc.DebugGlobalPose[parent][3]), 1.0f));
					const glm::vec3 p1 = glm::vec3(transform * glm::vec4(glm::vec3(skmc.DebugGlobalPose[j][3]), 1.0f));
					SceneRenderer::SubmitLine(p0, p1, color);
				}
			}
		}

		if (!ctx.ShowPhysicsColliders)
			return;

		// Box Colliders
		{
			auto view = ctx.ActiveScene->GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>();
			for (auto entity : view)
			{
				auto [tc, bc2d] = view.get<TransformComponent, BoxCollider2DComponent>(entity);

				glm::vec3 translation = tc.Translation + glm::vec3(bc2d.Offset, 0.001f);
				glm::vec3 scale = tc.Scale * glm::vec3(bc2d.Size * 2.0f, 1.0f);

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
					* glm::rotate(glm::mat4(1.0f), tc.Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f))
					* glm::scale(glm::mat4(1.0f), scale);

				SceneRenderer::SubmitRect(transform, glm::vec4(0, 1, 0, 1));
			}
		}

		// Circle Colliders: 32-segment polyline ring (replaces the old
		// thickness-0.01 SDF ring; visually equivalent, solid color, no
		// transparency overhead).
		{
			auto view = ctx.ActiveScene->GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>();
			for (auto entity : view)
			{
				auto [tc, cc2d] = view.get<TransformComponent, CircleCollider2DComponent>(entity);

				glm::vec3 translation = tc.Translation + glm::vec3(cc2d.Offset, 0.001f);
				glm::vec3 scale = tc.Scale * glm::vec3(cc2d.Radius * 2.0f);

				glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
					* glm::scale(glm::mat4(1.0f), scale);

				const glm::vec4 color(0, 1, 0, 1);
				constexpr uint32_t segments = 32;
				constexpr float twoPi = 6.28318530718f;
				for (uint32_t i = 1; i <= segments; ++i)
				{
					const float a0 = (i - 1) * twoPi / static_cast<float>(segments);
					const float a1 = i * twoPi / static_cast<float>(segments);
					const glm::vec3 p0 = glm::vec3(transform * glm::vec4(0.5f * glm::cos(a0), 0.5f * glm::sin(a0), 0.0f, 1.0f));
					const glm::vec3 p1 = glm::vec3(transform * glm::vec4(0.5f * glm::cos(a1), 0.5f * glm::sin(a1), 0.0f, 1.0f));
					SceneRenderer::SubmitLine(p0, p1, color);
				}
			}
		}
	}

	void GameFrameRenderer::RenderCameraPreview(const EditorRenderContext& ctx)
	{
		Entity previewEntity = ctx.PreviewEntity; // mutable copy — Entity is a registry handle
		auto& cameraComp = previewEntity.GetComponent<CameraComponent>();
		auto& cameraTransform = previewEntity.GetComponent<TransformComponent>();

		// Set camera viewport to match preview framebuffer size
		auto& sceneCamera = cameraComp.Camera;
		sceneCamera.SetViewportSize(ctx.PreviewTarget->GetWidth(), ctx.PreviewTarget->GetHeight());

		Ref<Framebuffer> hdrPreview = EnsureHDRTarget(s_HDRPreviewTarget,
			ctx.PreviewTarget->GetWidth(), ctx.PreviewTarget->GetHeight(), false);
		if (!hdrPreview)
			return;

		hdrPreview->Bind();
		SceneRenderer::SetActiveRenderTarget(hdrPreview);

		ctx.ActiveScene->RenderSceneFromCamera(cameraComp, cameraTransform.GetTransform());
		SceneRenderer::EndFrame();

		// HDR → LDR tonemap into the display preview target (ImGui samples it).
		TonemapPass(ctx.PreviewTarget.get(), s_HDRPreviewTarget);

		ctx.PreviewTarget->Unbind();
		ctx.ViewportTarget->Bind(); // Re-bind main FBO for subsequent UI rendering

		// Restore the main viewport HDR target as the active render target.
		SceneRenderer::SetActiveRenderTarget(s_HDRSceneTarget);
		// Restore camera viewport to main viewport size
		sceneCamera.SetViewportSize(ctx.ViewportTarget->GetWidth(), ctx.ViewportTarget->GetHeight());
	}

	void GameFrameRenderer::RenderUITo(Framebuffer& target, Scene& scene, float mouseX, float mouseY, bool mouseDown, float deltaTime)
	{
		ImGuiLayer* imgui = Application::Get().GetImGuiLayer();

		// Target is already bound by caller
		imgui->BeginGameUI((float)target.GetWidth(), (float)target.GetHeight(), mouseX, mouseY, mouseDown, deltaTime);
		UISystem::RenderUI(scene);
		// Composite the game UI into the target framebuffer (not the swap chain).
		imgui->EndGameUI(&target); // Render + RenderDrawData + switch back to editor context
		// Note: caller handles unbinding
	}

}
