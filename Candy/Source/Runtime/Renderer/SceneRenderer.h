#pragma once

#include "Runtime/Renderer/Camera.h"
#include "Runtime/Renderer/EditorCamera.h"
#include "Runtime/Renderer/Texture.h"
#include "Runtime/RHI/RHI.h"
#include "Runtime/Asset/StaticMeshResource.h"
#include "Runtime/Asset/Material.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Candy {

	// =========================================================================
	// SceneView — per-frame view data (simplified UE FViewInfo).
	// Layout matches CameraCB in PBR.hlsl (b0).
	// =========================================================================
	struct SceneView
	{
		glm::mat4 ViewProjection = glm::mat4(1.0f);
		glm::vec3 CameraPosition = glm::vec3(0.0f);
		float     Padding        = 1.0f; // cbuffer 16B alignment pad
	};

	// =========================================================================
	// DrawMaterialOverrides — per-draw material parameter overrides
	//
	// Sprites/circles are "procedural mesh + built-in material" draws, but each
	// entity has its own color/texture/tiling/circle params. The built-in
	// Sprite/Circle materials hold defaults; these overrides are applied on top
	// at render time (empty fields fall back to the material's defaults).
	// =========================================================================
	struct DrawMaterialOverrides
	{
		glm::vec4   BaseColor   = glm::vec4(1.0f);
		Ref<Texture2D> BaseColorTexture;     // non-null wins over BaseColorMap (runtime texture)
		std::string BaseColorMap;            // VFS:// path; empty = material default
		glm::vec2   UVTiling    = glm::vec2(1.0f);
		glm::vec2   UVOffset    = glm::vec2(0.0f);
		float       CircleMode  = 0.0f;      // 0 = plain Unlit (sprite), 1 = SDF circle
		float       Thickness   = 1.0f;      // circle ring thickness (1 = solid disc)
		float       Fade        = 0.005f;
	};

	// =========================================================================
	// MeshDrawCommand — one draw batch (simplified UE FMeshBatch).
	// References a static mesh, a submesh range, an optional material and the
	// world transform. Pure CPU data; GPU resources are resolved at submit.
	// =========================================================================
	struct MeshDrawCommand
	{
		glm::mat4 Transform = glm::mat4(1.0f);
		Ref<StaticMeshResource> Mesh;
		Ref<Material> Material;          // null = default material
		uint32_t SubmeshIndex = 0;
		int      EntityID = -1;          // picking ID (SV_TARGET1)

		/// 2D z-order key (transparent pass). FLT_MAX = 3D distance-sorted draw;
		/// otherwise smaller key = drawn first = further back (camera looks -Z).
		float SortKey = FLT_MAX;

		/// Per-draw material overrides (sprite/circle); false = material only.
		bool HasOverrides = false;
		DrawMaterialOverrides Overrides;
	};

	// =========================================================================
	// SceneRenderer — per-frame scene renderer for static meshes
	//
	// UE-flavored pipeline in minimal form: collect (Submit) -> cull/sort ->
	// render passes (EndFrame). Opaque/Masked draws go first (front-to-back by
	// material to minimize state switches), Transparent draws last
	// (back-to-front by camera distance, depth-write off, alpha blend on),
	// sprites/circles are 2D mesh draws with explicit z SortKeys, and the
	// debug-line overlay (collider wireframes) renders inside the same pass.
	//
	// D3D12-only for now: Init() warns and disables rendering on other
	// backends.
	// =========================================================================
	class SceneRenderer
	{
	public:
		static void Init();
		static void Shutdown();

		// ---- Per-frame pipeline (collect -> cull/sort -> passes) ----------
		static void BeginFrame(const SceneView& view);
		static void BeginFrame(const EditorCamera& camera);
		static void BeginFrame(const Camera& camera, const glm::mat4& transform);
		static void Submit(const MeshDrawCommand& cmd);

		// ---- Debug line pass (editor collider wireframes, overlay) --------
		static void SubmitLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityID = -1);
		/// Unit-quad corners ([-0.5, 0.5]^2 in the XY plane) transformed by
		/// `transform`, submitted as 4 lines (wireframe rectangle).
		static void SubmitRect(const glm::mat4& transform, const glm::vec4& color, int entityID = -1);

		/// Culls, sorts and submits all collected draws. Returns true when a
		/// render pass actually cleared the active target this frame (so the
		/// caller knows later passes load instead of clear again).
		static bool EndFrame();

		/// Sets the render-target framebuffer for the next EndFrame (same
		/// pending-clear semantics as before: the first pass after (re-)binding
		/// clears, subsequent passes load).
		static void SetActiveRenderTarget(const Ref<RHIFramebuffer>& fb);

		struct Statistics
		{
			uint32_t DrawCalls = 0;   // actually issued GPU draws
			uint32_t Submitted = 0;   // draw commands after frustum culling
		};
		static void ResetStats();
		static Statistics GetStats();

	private:
		static void RenderPass(RHICommandBuffer* cmd, const std::vector<MeshDrawCommand>& draws, bool transparent, uint32_t& nextSlice);
	};

} // namespace Candy
