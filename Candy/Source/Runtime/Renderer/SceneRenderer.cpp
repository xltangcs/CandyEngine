#include "CandyPCH.h"

#include "Runtime/Renderer/SceneRenderer.h"
#include "Runtime/Renderer/Renderer.h"
#include "Runtime/Renderer/Texture.h"
#include "Runtime/Core/FileSystem.h"
#include "Runtime/RHI/RHICommandQueue.h"
#include "Runtime/RHI/RHIContext.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <string>
#include <unordered_map>

namespace Candy {

	// ---- GPU constant layouts (byte-for-byte with PBR.hlsl / Sprite.hlsl) --

	// CameraCB (b0): float4x4 u_ViewProjection + float3 u_CameraPosition
	struct CameraUniforms
	{
		glm::mat4 ViewProjection = glm::mat4(1.0f);
		glm::vec3 CameraPosition = glm::vec3(0.0f);
		float     _Pad = 1.0f;
	};
	static_assert(sizeof(CameraUniforms) == 80, "CameraUniforms must match CameraCB (80B)");

	// MaterialCB (b1) — 3D PBR layout (PBR.hlsl): u_World + surface params +
	// flags + entity id. The 2D sprite layout lives in SpriteUniforms below.
	struct MaterialUniforms
	{
		glm::mat4  World             = glm::mat4(1.0f);
		glm::vec4  BaseColor         = glm::vec4(1.0f);
		float      Metallic          = 0.0f;
		float      Roughness         = 0.5f;
		float      BlendMode         = 0.0f;  // 0 Opaque, 1 Masked, 2 Transparent
		float      ShadingModel      = 0.0f;  // 0 Lit, 1 Unlit
		glm::vec3  Emissive          = glm::vec3(0.0f);
		float      EmissiveIntensity = 0.0f;
		glm::vec2  UVTiling          = glm::vec2(1.0f);
		glm::vec2  UVOffset          = glm::vec2(0.0f);
		uint32_t   TextureFlags      = 0;     // bit0 base, bit1 mr, bit2 normal, bit3 emissive
		int32_t    EntityID          = -1;
	};
	static_assert(sizeof(MaterialUniforms) == 136, "MaterialUniforms must match PBR.hlsl MaterialCB (136B)");

	// MaterialCB (b1) — 2D sprite layout (Sprite.hlsl): no lighting/TBN
	// surface params; SDF circle fields replace the PBR ones.
	struct SpriteUniforms
	{
		glm::mat4  World             = glm::mat4(1.0f);
		glm::vec4  BaseColor         = glm::vec4(1.0f);
		float      BlendMode         = 0.0f;  // 0 Opaque, 1 Masked, 2 Transparent
		float      CircleMode        = 0.0f;  // 0 = plain sprite, 1 = SDF circle
		float      Thickness         = 1.0f;  // circle ring thickness (1 = solid disc)
		float      Fade              = 0.005f;
		uint32_t   TextureFlags      = 0;     // bit0 base
		int32_t    EntityID          = -1;
		glm::vec2  UVTiling          = glm::vec2(1.0f);
		glm::vec2  UVOffset          = glm::vec2(0.0f);
	};
	static_assert(sizeof(SpriteUniforms) == 120, "SpriteUniforms must match Sprite.hlsl MaterialCB (120B)");

	// LightCB (b2): fixed light array + ambient. Byte-for-byte with PBR.hlsl.
	struct GPULight
	{
		glm::vec3 Direction   = glm::vec3(0.0f);
		float     Type        = 0.0f;  // 0 Directional, 1 Point, 2 Spot
		glm::vec3 Position    = glm::vec3(0.0f);
		float     Range       = 0.0f;
		glm::vec3 Color       = glm::vec3(0.0f);
		float     Intensity   = 0.0f;
		glm::vec2 ConeCos     = glm::vec2(1.0f, 0.0f); // (inner, outer)
		glm::vec2 _Pad        = glm::vec2(0.0f);
	};
	static_assert(sizeof(GPULight) == 64, "GPULight must match PBR.hlsl LightData (64B)");

	struct LightUniforms
	{
		GPULight    Lights[SceneRenderer::kMaxLights];
		glm::vec3   AmbientColor = glm::vec3(0.03f);
		int32_t     NumLights    = 0;
	};
	static_assert(sizeof(LightUniforms) == SceneRenderer::kMaxLights * 64 + 16,
		"LightUniforms must match PBR.hlsl LightCB (1040B)");

	// ---- Frustum culling (Gribb-Hartmann) ----------------------------------

	struct FrustumPlane
	{
		glm::vec3 Normal   = glm::vec3(0.0f);
		float     Distance = 0.0f;
	};

	struct Frustum
	{
		FrustumPlane Planes[6];

		Frustum(const glm::mat4& clipMatrix)
		{
			glm::vec4 rows[4];
			for (int i = 0; i < 4; ++i)
				rows[i] = glm::vec4(clipMatrix[0][i], clipMatrix[1][i], clipMatrix[2][i], clipMatrix[3][i]);

			glm::vec4 raw[6] = {
				rows[3] + rows[0], // left
				rows[3] - rows[0], // right
				rows[3] + rows[1], // bottom
				rows[3] - rows[1], // top
				rows[3] + rows[2], // near
				rows[3] - rows[2]  // far
			};
			for (int i = 0; i < 6; ++i)
			{
				const float len = glm::length(glm::vec3(raw[i]));
				if (len > 1e-6f)
					raw[i] /= len;
				Planes[i].Normal   = glm::vec3(raw[i]);
				Planes[i].Distance = raw[i].w;
			}
		}

		// AABB (model space) transformed by `transform`, tested with the
		// p-vertex test: the AABB is outside the frustum iff for some plane the
		// corner most along the plane normal is behind it.
		bool IntersectsAABB(const MeshAABB& bounds, const glm::mat4& transform) const
		{
			const glm::vec3 min = bounds.Min, max = bounds.Max;
			glm::vec3 wmin(FLT_MAX), wmax(-FLT_MAX);
			for (int i = 0; i < 8; ++i)
			{
				glm::vec3 corner(
					(i & 1) ? max.x : min.x,
					(i & 2) ? max.y : min.y,
					(i & 4) ? max.z : min.z);
				const glm::vec3 world = glm::vec3(transform * glm::vec4(corner, 1.0f));
				wmin = glm::min(wmin, world);
				wmax = glm::max(wmax, world);
			}

			for (int i = 0; i < 6; ++i)
			{
				glm::vec3 pv = wmin;
				if (Planes[i].Normal.x >= 0.0f) pv.x = wmax.x;
				if (Planes[i].Normal.y >= 0.0f) pv.y = wmax.y;
				if (Planes[i].Normal.z >= 0.0f) pv.z = wmax.z;
				if (glm::dot(Planes[i].Normal, pv) + Planes[i].Distance < 0.0f)
					return false;
			}
			return true;
		}
	};

	// ---- Renderer state ----------------------------------------------------

	// Built-in 2D shader path: materials bound to Sprite.hlsl use the sprite
	// PSO + the 120B SpriteUniforms layout (exact match — no filename sniffing).
	static constexpr const char* kSpriteShaderPath = "VFS://Engine/Content/Shaders/D3D12/Sprite.hlsl";

	// Packed per-material GPU state (rebuilt every frame; textures cached).
	struct MaterialState
	{
		MaterialUniforms Uniforms;
		SpriteUniforms   Sprite;
		std::array<Ref<Texture2D>, 4> Textures;
		float BlendMode = 0.0f;
		bool  IsSprite = false; // bound to Sprite.hlsl -> sprite PSO + SpriteUniforms
	};

	// Debug line pass vertex (per-vertex color + picking ID).
	struct LineVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		int32_t   EntityID;
	};

	struct SceneRendererData
	{
		// Static resources
		Ref<RHIBuffer>           CameraCB;   // 256B allocation (D3D12 root CBV alignment)
		Ref<RHIBuffer>           MaterialCB; // 256B-aligned slices, one per draw (grows on demand)
		uint32_t                 MaterialCBSlices = 0; // capacity of MaterialCB in slices
		Ref<RHIBuffer>           LightCB;    // packed scene lights + ambient (PBR path only)
		Ref<RHIGraphicsPipeline> OpaquePipeline;
		Ref<RHIGraphicsPipeline> TransparentPipeline;
		Ref<RHIGraphicsPipeline> SpriteTransparentPipeline; // Sprite.hlsl (2D sprites/circles)
		Ref<RHIGraphicsPipeline> LinePipeline;
		Ref<Texture2D>           WhiteTexture;

		// Per-frame state
		SceneView View;
		std::vector<MeshDrawCommand> DrawCommands;
		std::vector<LineVertex>      LineVertices;
		std::unordered_map<const Material*, MaterialState> MaterialStates;
		Ref<RHIFramebuffer> ActiveRenderTarget;
		bool ActiveRenderTargetPendingClear = true;

		// Per-frame lights (submitted like draws, packed into LightCB at EndFrame)
		std::array<SceneLight, SceneRenderer::kMaxLights> Lights;
		uint32_t LightCount = 0;
		glm::vec3 AmbientColor = glm::vec3(0.03f);

		// Debug line dynamic vertex buffer (grows on demand)
		Ref<RHIBuffer> LineVB;
		uint32_t       LineVBCapacity = 0; // in vertices

		// Cross-frame caches
		struct MeshGPU
		{
			Ref<RHIBuffer> VB;
			Ref<RHIBuffer> IB;
		};
		std::unordered_map<Ref<StaticMeshResource>, MeshGPU> MeshCache; // key holds Ref alive
		std::unordered_map<std::string, Ref<Texture2D>>      TextureCache; // VFS path -> texture

		SceneRenderer::Statistics Stats;
	};

	static SceneRendererData s_Data;

	// ---- Helpers -----------------------------------------------------------

	template<typename T>
	bool TryGetParam(const std::vector<ShaderParameter>& params, const std::string& name, T& out)
	{
		for (const auto& p : params)
		{
			if (p.Name != name)
				continue;

			if constexpr (std::is_same_v<T, float>)
			{
				if (std::holds_alternative<float>(p.Default)) { out = std::get<float>(p.Default); return true; }
				if (std::holds_alternative<int>(p.Default))   { out = static_cast<float>(std::get<int>(p.Default)); return true; }
			}
			else if constexpr (std::is_same_v<T, glm::vec4>)
			{
				if (std::holds_alternative<glm::vec4>(p.Default)) { out = std::get<glm::vec4>(p.Default); return true; }
				if (std::holds_alternative<glm::vec3>(p.Default)) { out = glm::vec4(std::get<glm::vec3>(p.Default), 1.0f); return true; }
			}
			else if constexpr (std::is_same_v<T, glm::vec3>)
			{
				if (std::holds_alternative<glm::vec3>(p.Default)) { out = std::get<glm::vec3>(p.Default); return true; }
			}
			else if constexpr (std::is_same_v<T, glm::vec2>)
			{
				if (std::holds_alternative<glm::vec2>(p.Default)) { out = std::get<glm::vec2>(p.Default); return true; }
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				if (std::holds_alternative<std::string>(p.Default)) { out = std::get<std::string>(p.Default); return true; }
			}
			return false;
		}
		return false;
	}

	Ref<Texture2D> GetTexture(const std::string& vfsPath)
	{
		if (vfsPath.empty())
			return s_Data.WhiteTexture;

		auto it = s_Data.TextureCache.find(vfsPath);
		if (it != s_Data.TextureCache.end())
			return it->second;

		Ref<Texture2D> tex = Texture2D::Create(vfsPath);
		if (!tex)
			tex = s_Data.WhiteTexture;
		s_Data.TextureCache[vfsPath] = tex;
		return tex;
	}

	const SceneRendererData::MeshGPU& GetMeshGPU(const Ref<StaticMeshResource>& mesh)
	{
		auto it = s_Data.MeshCache.find(mesh);
		if (it != s_Data.MeshCache.end())
			return it->second;

		auto* dev = RHIContext::GetDevice();
		SceneRendererData::MeshGPU gpu;

		BufferDesc vb;
		vb.Size          = mesh->Vertices.size() * sizeof(MeshVertex);
		vb.Usage         = ResourceUsage::VertexBuffer;
		vb.CPUAccessible = true; // simple upload path (D3D12 upload heap)
		vb.Stride        = sizeof(MeshVertex);
		vb.DebugName     = "SceneRenderer_MeshVB";
		gpu.VB = dev->CreateBuffer(vb);
		if (gpu.VB && !mesh->Vertices.empty())
			gpu.VB->Write(mesh->Vertices.data(), mesh->Vertices.size() * sizeof(MeshVertex));

		BufferDesc ib;
		ib.Size          = mesh->Indices.size() * sizeof(uint32_t);
		ib.Usage         = ResourceUsage::IndexBuffer;
		ib.CPUAccessible = true;
		ib.DebugName     = "SceneRenderer_MeshIB";
		gpu.IB = dev->CreateBuffer(ib);
		if (gpu.IB && !mesh->Indices.empty())
			gpu.IB->Write(mesh->Indices.data(), mesh->Indices.size() * sizeof(uint32_t));

		auto [entry, inserted] = s_Data.MeshCache.emplace(mesh, std::move(gpu));
		(void)inserted;
		return entry->second;
	}

	const MaterialState& GetMaterialState(const Ref<Material>& material)
	{
		const Material* key = material.get();
		auto it = s_Data.MaterialStates.find(key);
		if (it != s_Data.MaterialStates.end())
			return it->second;

		MaterialState state;
		state.Uniforms = MaterialUniforms();
		state.Sprite   = SpriteUniforms();
		state.Textures = { s_Data.WhiteTexture, s_Data.WhiteTexture, s_Data.WhiteTexture, s_Data.WhiteTexture };
		state.BlendMode = 0.0f;

		if (material)
		{
			// Sprite.hlsl materials (Builtin/Sprite, Builtin/Circle, user .mat)
			// use the dedicated unlit 2D PSO + SpriteUniforms; everything else
			// uses the PBR pipelines + MaterialUniforms.
			state.IsSprite = material->ShaderPath == kSpriteShaderPath;

			std::vector<ShaderParameter> params;
			if (material->GetShaderParameters(params))
			{
				if (state.IsSprite)
				{
					TryGetParam(params, "u_BaseColor",   state.Sprite.BaseColor);
					TryGetParam(params, "u_BlendMode",   state.Sprite.BlendMode);
					TryGetParam(params, "u_UVTiling",    state.Sprite.UVTiling);
					TryGetParam(params, "u_UVOffset",    state.Sprite.UVOffset);
					TryGetParam(params, "u_CircleMode",  state.Sprite.CircleMode);
					TryGetParam(params, "u_Thickness",   state.Sprite.Thickness);
					TryGetParam(params, "u_Fade",        state.Sprite.Fade);
					state.BlendMode = state.Sprite.BlendMode;
				}
				else
				{
					TryGetParam(params, "u_BaseColor",         state.Uniforms.BaseColor);
					TryGetParam(params, "u_Metallic",          state.Uniforms.Metallic);
					TryGetParam(params, "u_Roughness",         state.Uniforms.Roughness);
					TryGetParam(params, "u_BlendMode",         state.Uniforms.BlendMode);
					TryGetParam(params, "u_ShadingModel",      state.Uniforms.ShadingModel);
					TryGetParam(params, "u_Emissive",          state.Uniforms.Emissive);
					TryGetParam(params, "u_EmissiveIntensity", state.Uniforms.EmissiveIntensity);
					TryGetParam(params, "u_UVTiling",          state.Uniforms.UVTiling);
					TryGetParam(params, "u_UVOffset",          state.Uniforms.UVOffset);
					state.BlendMode = state.Uniforms.BlendMode;
				}

				std::string path;
				uint32_t flags = 0;
				if (TryGetParam(params, "u_BaseColorMap", path))
				{
					state.Textures[0] = GetTexture(path);
					if (!path.empty()) flags |= 1;
				}
				if (state.IsSprite)
				{
					state.Sprite.TextureFlags = flags;
				}
				else
				{
					if (TryGetParam(params, "u_MetallicRoughnessMap", path))
					{
						state.Textures[1] = GetTexture(path);
						if (!path.empty()) flags |= 2;
					}
					if (TryGetParam(params, "u_NormalMap", path))
					{
						state.Textures[2] = GetTexture(path);
						if (!path.empty()) flags |= 4;
					}
					if (TryGetParam(params, "u_EmissiveMap", path))
					{
						state.Textures[3] = GetTexture(path);
						if (!path.empty()) flags |= 8;
					}
					state.Uniforms.TextureFlags = flags;
				}
			}
		}

		auto [entry, inserted] = s_Data.MaterialStates.emplace(key, std::move(state));
		(void)inserted;
		return entry->second;
	}

	// ---- SceneRenderer ---------------------------------------------------------

	void SceneRenderer::Init()
	{
		if (Renderer::GetAPI() != RendererAPI::API::D3D12)
		{
			CANDY_CORE_WARN("SceneRenderer: only the D3D12 backend is supported at this stage; mesh rendering disabled");
			return;
		}

		auto* dev = RHIContext::GetDevice();
		if (!dev)
		{
			CANDY_CORE_ERROR("SceneRenderer: no RHI device published");
			return;
		}

		// White texture (fallback for empty texture slots)
		s_Data.WhiteTexture = Texture2D::Create(1, 1);
		uint32_t white = 0xffffffff;
		s_Data.WhiteTexture->SetData(&white, sizeof(white));

		// Constant buffers (D3D12 root CBV requires 256B-aligned allocation size)
		{
			BufferDesc cb;
			cb.Size          = 256;
			cb.Usage         = ResourceUsage::ConstantBuffer;
			cb.CPUAccessible = true;
			cb.DebugName     = "SceneRenderer_CameraCB";
			s_Data.CameraCB = dev->CreateBuffer(cb);
		}

		// LightCB: 1040B of packed lights + ambient, rounded up to 256B
		// alignment (D3D12 root CBV allocation rule).
		{
			BufferDesc cb;
			cb.Size          = 1280;
			cb.Usage         = ResourceUsage::ConstantBuffer;
			cb.CPUAccessible = true;
			cb.DebugName     = "SceneRenderer_LightCB";
			s_Data.LightCB = dev->CreateBuffer(cb);
		}

		// MaterialCB: one 256B-aligned slice per draw (grows on demand in EndFrame).
		s_Data.MaterialCB.reset();
		s_Data.MaterialCBSlices = 0;

		// Shader modules (single multi-stage file)
		auto loadShader = [&](ShaderStage stage, const char* entry, const char* debugName) -> Ref<RHIShaderModule> {
			const char* path = "VFS://Engine/Content/Shaders/D3D12/PBR.hlsl";
			auto src = FileSystem::Get().ReadText(path);
			if (!src)
			{
				CANDY_CORE_ERROR("SceneRenderer: failed to load shader '{}'", path);
				return nullptr;
			}
			return dev->CreateShaderModuleFromSource(src->c_str(), stage, entry, debugName);
		};
		auto vs = loadShader(ShaderStage::Vertex,   "VSMain", "PBR_VS");
		auto ps = loadShader(ShaderStage::Fragment, "PSMain", "PBR_PS");
		if (!vs || !ps)
			return;

		// Pipelines (cached by RHIPipelineCache on desc hash)
		GraphicsPipelineDesc base;
		base.Topology            = PrimitiveTopology::Triangles;
		base.Rasterizer.Cull     = CullMode::Back;
		base.Rasterizer.Fill     = FillMode::Solid;
		// glTF/OpenGL convention: counter-clockwise triangles are front-facing.
		base.Rasterizer.FrontCounterClockwise = true;
		base.DepthStencil.DepthTestEnable  = true;
		base.DepthStencil.DepthWriteEnable = true;
		base.DepthStencil.DepthCompareOp   = CompareOp::Less;
		base.DepthStencilFormat = RHIFormat::D24UnormS8Uint;
		base.Blend.BlendEnable  = false;
		base.RenderTargetFormats = { RHIFormat::R8G8B8A8Unorm, RHIFormat::R32Sint };

		VertexInputLayout::VertexBinding binding;
		binding.Binding = 0;
		binding.Stride  = sizeof(MeshVertex);
		base.VertexInput.Bindings.push_back(binding);
		base.VertexInput.Attributes.push_back({ 0, 0, RHIFormat::R32G32B32Float,    offsetof(MeshVertex, Position) });
		base.VertexInput.Attributes.push_back({ 1, 0, RHIFormat::R32G32B32Float,    offsetof(MeshVertex, Normal) });
		base.VertexInput.Attributes.push_back({ 2, 0, RHIFormat::R32G32B32A32Float, offsetof(MeshVertex, Tangent) });
		base.VertexInput.Attributes.push_back({ 3, 0, RHIFormat::R32G32Float,       offsetof(MeshVertex, TexCoord) });

		s_Data.OpaquePipeline = dev->CreateGraphicsPipeline(base, vs, ps);
		if (!s_Data.OpaquePipeline)
		{
			CANDY_CORE_ERROR("SceneRenderer: failed to create opaque pipeline; mesh rendering disabled");
			return;
		}

		GraphicsPipelineDesc transparent = base;
		transparent.DepthStencil.DepthWriteEnable = false;
		transparent.Blend.BlendEnable = true;
		transparent.Blend.SrcColorBlendFactor = BlendState::BlendFactor::SrcAlpha;
		transparent.Blend.DstColorBlendFactor = BlendState::BlendFactor::OneMinusSrcAlpha;
		s_Data.TransparentPipeline = dev->CreateGraphicsPipeline(transparent, vs, ps);
		if (!s_Data.TransparentPipeline)
		{
			CANDY_CORE_ERROR("SceneRenderer: failed to create transparent pipeline; mesh rendering disabled");
			return;
		}

		// ---- Sprite pipeline (2D sprites/circles, Sprite.hlsl) -------------
		// Same transparent desc as PBR, but a dedicated unlit shader: no
		// lighting/TBN, and its own //@param reflection (clean material
		// inspector without PBR noise). RHIPipelineCache keys on desc +
		// shader contents, so this produces a distinct PSO.
		{
			const char* path = "VFS://Engine/Content/Shaders/D3D12/Sprite.hlsl";
			auto src = FileSystem::Get().ReadText(path);
			if (!src)
			{
				CANDY_CORE_ERROR("SceneRenderer: failed to load shader '{}'", path);
			}
			else
			{
				auto spriteVS = dev->CreateShaderModuleFromSource(src->c_str(), ShaderStage::Vertex,   "VSMain", "Sprite_VS");
				auto spritePS = dev->CreateShaderModuleFromSource(src->c_str(), ShaderStage::Fragment, "PSMain", "Sprite_PS");
				if (!spriteVS || !spritePS)
				{
					CANDY_CORE_ERROR("SceneRenderer: failed to compile Sprite.hlsl");
				}
				else
				{
					s_Data.SpriteTransparentPipeline = dev->CreateGraphicsPipeline(transparent, spriteVS, spritePS);
					if (!s_Data.SpriteTransparentPipeline)
						CANDY_CORE_ERROR("SceneRenderer: failed to create sprite pipeline");
				}
			}
		}

		// ---- Debug line pipeline (editor collider wireframes, overlay) ----
		// Topology=Lines, cull none, depth test off (always visible — same
		// semantics as the previous overlay), alpha blend on.
		// DepthStencilFormat must match the framebuffer (D3D12 PSO/RT format
		// match), even though the test is disabled.
		{
			const char* path = "VFS://Engine/Content/Shaders/D3D12/DebugLine.hlsl";
			auto src = FileSystem::Get().ReadText(path);
			if (!src)
			{
				CANDY_CORE_ERROR("SceneRenderer: failed to load shader '{}'", path);
			}
			else
			{
				auto lineVS = dev->CreateShaderModuleFromSource(src->c_str(), ShaderStage::Vertex,   "VSMain", "DebugLine_VS");
				auto linePS = dev->CreateShaderModuleFromSource(src->c_str(), ShaderStage::Fragment, "PSMain", "DebugLine_PS");
				if (!lineVS || !linePS)
				{
					CANDY_CORE_ERROR("SceneRenderer: failed to compile DebugLine.hlsl");
				}
				else
				{
					GraphicsPipelineDesc ld;
					ld.Topology            = PrimitiveTopology::Lines;
					ld.Rasterizer.Cull     = CullMode::None;
					ld.Rasterizer.Fill     = FillMode::Solid;
					ld.Rasterizer.FrontCounterClockwise = true;
					ld.DepthStencil.DepthTestEnable  = false;
					ld.DepthStencil.DepthWriteEnable = false;
					ld.DepthStencil.DepthCompareOp   = CompareOp::Less;
					ld.DepthStencilFormat = RHIFormat::D24UnormS8Uint;
					ld.Blend.BlendEnable  = true;
					ld.Blend.SrcColorBlendFactor = BlendState::BlendFactor::SrcAlpha;
					ld.Blend.DstColorBlendFactor = BlendState::BlendFactor::OneMinusSrcAlpha;
					ld.RenderTargetFormats = { RHIFormat::R8G8B8A8Unorm, RHIFormat::R32Sint };

					VertexInputLayout::VertexBinding lb;
					lb.Binding = 0;
					lb.Stride  = sizeof(LineVertex);
					ld.VertexInput.Bindings.push_back(lb);
					ld.VertexInput.Attributes.push_back({ 0, 0, RHIFormat::R32G32B32Float,    offsetof(LineVertex, Position) });
					ld.VertexInput.Attributes.push_back({ 1, 0, RHIFormat::R32G32B32A32Float, offsetof(LineVertex, Color) });
					ld.VertexInput.Attributes.push_back({ 2, 0, RHIFormat::R32Sint,           offsetof(LineVertex, EntityID) });

					s_Data.LinePipeline = dev->CreateGraphicsPipeline(ld, lineVS, linePS);
					if (!s_Data.LinePipeline)
						CANDY_CORE_ERROR("SceneRenderer: failed to create debug line pipeline");
				}
			}
		}

		CANDY_CORE_INFO("SceneRenderer: initialized ({} backend)", RendererAPI::StringFromAPI(Renderer::GetAPI()));
	}

	void SceneRenderer::Shutdown()
	{
		s_Data.CameraCB.reset();
		s_Data.MaterialCB.reset();
		s_Data.LightCB.reset();
		s_Data.OpaquePipeline.reset();
		s_Data.TransparentPipeline.reset();
		s_Data.SpriteTransparentPipeline.reset();
		s_Data.LinePipeline.reset();
		s_Data.LineVB.reset();
		s_Data.LineVBCapacity = 0;
		s_Data.WhiteTexture.reset();
		s_Data.ActiveRenderTarget.reset();
		s_Data.ActiveRenderTargetPendingClear = true;
		s_Data.MeshCache.clear();
		s_Data.TextureCache.clear();
		s_Data.DrawCommands.clear();
		s_Data.LineVertices.clear();
		s_Data.MaterialStates.clear();
	}

	void SceneRenderer::BeginFrame(const SceneView& view)
	{
		s_Data.View = view;
		s_Data.DrawCommands.clear();
		s_Data.LineVertices.clear();
		s_Data.MaterialStates.clear();
		s_Data.LightCount = 0;
		s_Data.Stats = {};
	}

	void SceneRenderer::BeginFrame(const EditorCamera& camera)
	{
		SceneView view;
		view.ViewProjection = camera.GetViewProjection();
		view.CameraPosition = camera.GetPosition();
		BeginFrame(view);
	}

	void SceneRenderer::BeginFrame(const Camera& camera, const glm::mat4& transform)
	{
		SceneView view;
		view.ViewProjection = camera.GetProjection() * glm::inverse(transform);
		view.CameraPosition = glm::vec3(transform[3]);
		BeginFrame(view);
	}

	void SceneRenderer::Submit(const MeshDrawCommand& cmd)
	{
		s_Data.DrawCommands.push_back(cmd);
	}

	void SceneRenderer::SubmitLight(const SceneLight& light)
	{
		if (s_Data.LightCount >= kMaxLights)
		{
			CANDY_CORE_WARN("SceneRenderer: light count exceeded max ({0}); extra lights ignored", kMaxLights);
			return;
		}
		s_Data.Lights[s_Data.LightCount++] = light;
	}

	void SceneRenderer::SetAmbientLight(const glm::vec3& color)
	{
		s_Data.AmbientColor = color;
	}

	void SceneRenderer::SubmitLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityID)
	{
		s_Data.LineVertices.push_back({ p0, color, entityID });
		s_Data.LineVertices.push_back({ p1, color, entityID });
	}

	void SceneRenderer::SubmitRect(const glm::mat4& transform, const glm::vec4& color, int entityID)
	{
		// Unit-quad corners ([-0.5, 0.5]^2, XY plane) — matches the quad mesh
		// convention sprites use, so a wireframe overlays the shape exactly.
		const glm::vec3 corners[4] = {
			glm::vec3(transform * glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f)),
			glm::vec3(transform * glm::vec4( 0.5f, -0.5f, 0.0f, 1.0f)),
			glm::vec3(transform * glm::vec4( 0.5f,  0.5f, 0.0f, 1.0f)),
			glm::vec3(transform * glm::vec4(-0.5f,  0.5f, 0.0f, 1.0f)),
		};
		for (int i = 0; i < 4; ++i)
			SubmitLine(corners[i], corners[(i + 1) % 4], color, entityID);
	}

	bool SceneRenderer::EndFrame()
	{
		auto* dev = RHIContext::GetDevice();
		if (!dev || !s_Data.OpaquePipeline || !s_Data.CameraCB)
		{
			s_Data.DrawCommands.clear();
			s_Data.MaterialStates.clear();
			return false;
		}

		// Upload view constants
		CameraUniforms camera;
		camera.ViewProjection = s_Data.View.ViewProjection;
		camera.CameraPosition = s_Data.View.CameraPosition;
		if (!s_Data.CameraCB->Write(&camera, sizeof(camera)))
		{
			CANDY_CORE_ERROR("SceneRenderer::EndFrame - CameraCB upload failed");
			s_Data.DrawCommands.clear();
			s_Data.MaterialStates.clear();
			return false;
		}

		// Upload packed scene lights + ambient (PBR path only; sprite/line
		// shaders never read b2).
		LightUniforms lights;
		for (uint32_t i = 0; i < s_Data.LightCount; ++i)
		{
			const SceneLight& src = s_Data.Lights[i];
			GPULight& dst         = lights.Lights[i];
			dst.Direction = src.Direction;
			dst.Type      = static_cast<float>(src.Type);
			dst.Position  = src.Position;
			dst.Range     = src.Range;
			dst.Color     = src.Color;
			dst.Intensity = src.Intensity;
			dst.ConeCos   = glm::vec2(src.InnerConeCos, src.OuterConeCos);
		}
		lights.AmbientColor = s_Data.AmbientColor;
		lights.NumLights    = static_cast<int32_t>(s_Data.LightCount);
		if (!s_Data.LightCB->Write(&lights, sizeof(lights)))
			CANDY_CORE_ERROR("SceneRenderer::EndFrame - LightCB upload failed");

		// Frustum cull + pass split (may produce zero draws — the render pass
		// below still runs so an empty scene clears the target; without this,
		// the previous flush-always-clears guarantee would be lost).
		const Frustum frustum(s_Data.View.ViewProjection);
		std::vector<MeshDrawCommand> opaqueDraws;
		std::vector<MeshDrawCommand> transparentDraws;
		opaqueDraws.reserve(s_Data.DrawCommands.size());

		for (const auto& draw : s_Data.DrawCommands)
		{
			if (!draw.Mesh || draw.SubmeshIndex >= draw.Mesh->Submeshes.size())
				continue;
			if (!frustum.IntersectsAABB(draw.Mesh->Bounds, draw.Transform))
				continue;

			const float blendMode = GetMaterialState(draw.Material).BlendMode;
			if (blendMode >= 1.5f)
				transparentDraws.push_back(draw);
			else
				opaqueDraws.push_back(draw);
			s_Data.Stats.Submitted++;
		}

		// Opaque: group by material pointer to minimize pipeline/state switches.
		std::stable_sort(opaqueDraws.begin(), opaqueDraws.end(),
			[](const MeshDrawCommand& a, const MeshDrawCommand& b) {
				return a.Material.get() < b.Material.get();
			});

		// Transparent: 2D sprite draws (explicit SortKey) come last and are
		// z-ordered (ascending SortKey = far to near), 3D draws are sorted
		// back-to-front by camera distance. stable_sort keeps submission order
		// for equal keys, so same-z sprites layer deterministically without an
		// explicit SortingOrder component.
		if (!transparentDraws.empty())
		{
			const glm::vec3 camPos = s_Data.View.CameraPosition;
			std::stable_sort(transparentDraws.begin(), transparentDraws.end(),
				[&](const MeshDrawCommand& a, const MeshDrawCommand& b) {
					const bool a2D = a.SortKey != FLT_MAX;
					const bool b2D = b.SortKey != FLT_MAX;
					if (a2D && b2D)
						return a.SortKey < b.SortKey;
					if (a2D != b2D)
						return b2D; // 3D distance group draws before the 2D z-key group
					auto center = [](const MeshDrawCommand& d) {
						const glm::vec3 c = (d.Mesh->Bounds.Min + d.Mesh->Bounds.Max) * 0.5f;
						return glm::vec3(d.Transform * glm::vec4(c, 1.0f));
					};
					const glm::vec3 diffA = center(a) - camPos;
					const glm::vec3 diffB = center(b) - camPos;
					return glm::dot(diffA, diffA) > glm::dot(diffB, diffB);
				});
		}

		// Grow the per-draw constant buffer to fit this frame's draw count.
		// Each draw owns a 256B slice; the GPU executes all draws after the
		// CPU finished recording, so per-draw slices are required to prevent
		// every draw reading the last one's uniforms.
		{
			const uint32_t drawCount = static_cast<uint32_t>(opaqueDraws.size() + transparentDraws.size());
			if (drawCount > s_Data.MaterialCBSlices)
			{
				const uint32_t slices = std::max(drawCount, 256u);
				BufferDesc cb;
				cb.Size          = static_cast<uint64_t>(slices) * 256;
				cb.Usage         = ResourceUsage::ConstantBuffer;
				cb.CPUAccessible = true;
				cb.DebugName     = "SceneRenderer_MaterialCB";
				s_Data.MaterialCB = dev->CreateBuffer(cb);
				s_Data.MaterialCBSlices = slices;
			}
		}

		// Record + submit
		auto& queue = dev->GetCommandQueue();
		auto cmd = queue.CreateCommandBuffer();
		if (!cmd)
		{
			s_Data.DrawCommands.clear();
			s_Data.MaterialStates.clear();
			return false;
		}

		cmd->Begin();

		// Clear only on the first pass after (re-)binding the target; later
		// passes in the same frame (2D overlay, camera preview) load instead.
		const LoadOp loadOp = (!s_Data.ActiveRenderTarget || s_Data.ActiveRenderTargetPendingClear)
			? LoadOp::Clear : LoadOp::Load;

		RenderPassDesc rpDesc;
		{
			RenderPassColorAttachment color;
			color.Format     = RHIFormat::R8G8B8A8Unorm;
			color.LoadOp     = loadOp;
			color.ClearColor[0] = 0.1f;
			color.ClearColor[1] = 0.1f;
			color.ClearColor[2] = 0.1f;
			color.ClearColor[3] = 1.0f;
			rpDesc.ColorAttachments.push_back(color);

			if (s_Data.ActiveRenderTarget && s_Data.ActiveRenderTarget->GetColorAttachmentCount() > 1)
			{
				RenderPassColorAttachment id;
				id.Format     = RHIFormat::R32Sint;
				id.LoadOp     = loadOp;
				id.ClearColor[0] = -1.0f;
				id.ClearColor[1] = -1.0f;
				id.ClearColor[2] = -1.0f;
				id.ClearColor[3] = -1.0f;
				rpDesc.ColorAttachments.push_back(id);
			}
		}
		cmd->BeginRenderPass(s_Data.ActiveRenderTarget.get(), rpDesc);
		s_Data.ActiveRenderTargetPendingClear = false;

		uint32_t vpW = 1280, vpH = 720;
		if (s_Data.ActiveRenderTarget)
		{
			vpW = s_Data.ActiveRenderTarget->GetWidth();
			vpH = s_Data.ActiveRenderTarget->GetHeight();
		}
		cmd->SetViewport(0, 0, static_cast<float>(vpW), static_cast<float>(vpH));
		cmd->SetScissor(0, 0, vpW, vpH);

		uint32_t nextSlice = 0;
		RenderPass(cmd.get(), opaqueDraws, false, nextSlice);
		RenderPass(cmd.get(), transparentDraws, true, nextSlice);

		// Debug line pass: after transparent, inside the same render pass
		// (overlay semantics, depth test off, per-vertex color + entity ID).
		if (!s_Data.LineVertices.empty() && s_Data.LinePipeline)
		{
			const uint32_t lineCount = static_cast<uint32_t>(s_Data.LineVertices.size());
			if (lineCount > s_Data.LineVBCapacity)
			{
				const uint32_t capacity = std::max(lineCount, 1024u);
				BufferDesc lb;
				lb.Size          = static_cast<uint64_t>(capacity) * sizeof(LineVertex);
				lb.Usage         = ResourceUsage::VertexBuffer;
				lb.CPUAccessible = true;
				lb.Stride        = sizeof(LineVertex);
				lb.DebugName     = "SceneRenderer_LineVB";
				s_Data.LineVB = dev->CreateBuffer(lb);
				s_Data.LineVBCapacity = capacity;
			}

			if (s_Data.LineVB && s_Data.LineVB->Write(s_Data.LineVertices.data(), lineCount * sizeof(LineVertex)))
			{
				cmd->SetPipeline(s_Data.LinePipeline);
				cmd->SetConstantBuffer(0, 0, s_Data.CameraCB);
				cmd->SetVertexBuffer(s_Data.LineVB);
				cmd->Draw(lineCount);
				s_Data.Stats.DrawCalls++;
			}
		}

		cmd->EndRenderPass();
		cmd->End();
		queue.Submit({ cmd.get() });
		dev->WaitIdle();

		s_Data.DrawCommands.clear();
		s_Data.MaterialStates.clear();
		return true;
	}

	void SceneRenderer::RenderPass(RHICommandBuffer* cmd, const std::vector<MeshDrawCommand>& draws, bool transparent, uint32_t& nextSlice)
	{
		if (draws.empty())
			return;

		for (const auto& draw : draws)
		{
			const MaterialState& state = GetMaterialState(draw.Material);

			// Sprite.hlsl materials use the dedicated unlit 2D PSO; everything
			// else uses the PBR pipelines. Re-set per draw (cheap, stable in
			// practice). The built-in sprite/circle materials are always
			// transparent; an opaque sprite material would render through the
			// sprite transparent PSO too (alpha=1 blends to the same result).
			Ref<RHIGraphicsPipeline> pipeline = state.IsSprite
				? s_Data.SpriteTransparentPipeline
				: (transparent ? s_Data.TransparentPipeline : s_Data.OpaquePipeline);
			if (!pipeline)
				pipeline = transparent ? s_Data.TransparentPipeline : s_Data.OpaquePipeline;
			cmd->SetPipeline(pipeline);

			// Root CBVs must be recorded with the pipeline's root signature
			// already bound — set them after SetPipeline, every draw.
			cmd->SetConstantBuffer(0, 0, s_Data.CameraCB);

			// Scene lights (b2): only the PBR pipelines' shaders declare the
			// register; the sprite/line root signatures simply ignore it.
			if (!state.IsSprite && s_Data.LightCB)
				cmd->SetConstantBuffer(3, 0, s_Data.LightCB);

			// 2D sprite draws pack SpriteUniforms (120B), 3D draws pack
			// MaterialUniforms (136B); each draw still owns a 256B-aligned slice.
			MaterialUniforms uniforms = state.Uniforms;
			SpriteUniforms   sprite   = state.Sprite;
			uniforms.World    = draw.Transform;
			sprite.World      = draw.Transform;
			uniforms.EntityID = draw.EntityID;
			sprite.EntityID   = draw.EntityID;

			// Per-draw SRV table contents (defaults = material's textures)
			std::array<Ref<RHITexture>, 4> textures;
			for (uint32_t i = 0; i < 4; ++i)
				textures[i] = state.Textures[i]->GetRHITexture();

			// Per-draw material overrides (sprites/circles): apply on top of
			// the shared built-in material's reflected defaults.
			if (draw.HasOverrides)
			{
				if (state.IsSprite)
				{
					sprite.BaseColor  = draw.Overrides.BaseColor;
					sprite.UVTiling   = draw.Overrides.UVTiling;
					sprite.UVOffset   = draw.Overrides.UVOffset;
					sprite.CircleMode = draw.Overrides.CircleMode;
					sprite.Thickness  = draw.Overrides.Thickness;
					sprite.Fade       = draw.Overrides.Fade;
				}
				else
				{
					uniforms.BaseColor = draw.Overrides.BaseColor;
					uniforms.UVTiling  = draw.Overrides.UVTiling;
					uniforms.UVOffset  = draw.Overrides.UVOffset;
				}
				if (draw.Overrides.BaseColorTexture)
				{
					textures[0] = draw.Overrides.BaseColorTexture->GetRHITexture();
					if (state.IsSprite) sprite.TextureFlags |= 1;
					else                uniforms.TextureFlags |= 1;
				}
				else if (!draw.Overrides.BaseColorMap.empty())
				{
					textures[0] = GetTexture(draw.Overrides.BaseColorMap)->GetRHITexture();
					if (state.IsSprite) sprite.TextureFlags |= 1;
					else                uniforms.TextureFlags |= 1;
				}
			}

			// Per-draw 256B-aligned slice: without it, all draws bound to the
			// same GPU address would read the last recorded uniforms at
			// execution time (root CBV binds a fixed address, and recording
			// happens before submission).
			const uint64_t cbOffset = static_cast<uint64_t>(nextSlice++) * 256;
			const bool uploaded = state.IsSprite
				? s_Data.MaterialCB->Write(&sprite, sizeof(sprite), cbOffset)
				: s_Data.MaterialCB->Write(&uniforms, sizeof(uniforms), cbOffset);
			if (!uploaded)
				CANDY_CORE_WARN("SceneRenderer::RenderPass - MaterialCB upload failed");

			cmd->SetConstantBuffer(1, 0, s_Data.MaterialCB, cbOffset);
			cmd->SetTextures(2, 4, textures.data());

			const auto& gpu = GetMeshGPU(draw.Mesh);
			cmd->SetVertexBuffer(gpu.VB);
			cmd->SetIndexBuffer(gpu.IB);

			const auto& submesh = draw.Mesh->Submeshes[draw.SubmeshIndex];
			cmd->DrawIndexed(submesh.IndexCount, 1, submesh.IndexOffset);
			s_Data.Stats.DrawCalls++;
		}
	}

	void SceneRenderer::SetActiveRenderTarget(const Ref<RHIFramebuffer>& fb)
	{
		s_Data.ActiveRenderTarget = fb;
		s_Data.ActiveRenderTargetPendingClear = true;
	}

	void SceneRenderer::ResetStats()
	{
		s_Data.Stats = {};
	}

	SceneRenderer::Statistics SceneRenderer::GetStats()
	{
		return s_Data.Stats;
	}

} // namespace Candy
