#include "CandyPCH.h"

#include "Runtime/Renderer/Renderer2D.h"
#include "Runtime/Renderer/Renderer.h"
#include "Runtime/Renderer/GraphicsContext.h"
#include "Runtime/Core/FileSystem.h"
#include "Runtime/Core/Application.h"
#include "Runtime/RHI/RHICommandQueue.h"
#include "Runtime/RHI/RHIContext.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Backend includes — needed only by the [FROZEN] Vulkan init branch.
// The unified D3D12/OpenGL init and Flush() go through RHIContext + RHI interfaces.
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanGraphicsContext.h"


namespace Candy {
	struct QuadVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
		float TexIndex;
		float TilingFactor;

		int EntityID;
	};

	struct CircleVertex
	{
		glm::vec3 WorldPosition;
		glm::vec3 LocalPosition;
		glm::vec4 Color;
		float Thickness;
		float Fade;

		// Editor-only
		int EntityID;
	};

	struct LineVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;

		// Editor-only
		int EntityID;
	};


	struct Renderer2DData
	{
		static const uint32_t MaxQuads = 20000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;
		static const uint32_t MaxTextureSlots = 32; // TODO: RenderCaps

		Ref<Texture2D> WhiteTexture;

		uint32_t QuadIndexCount = 0;
		QuadVertex* QuadVertexBufferBase = nullptr;
		QuadVertex* QuadVertexBufferPtr = nullptr;

		uint32_t CircleIndexCount = 0;
		CircleVertex* CircleVertexBufferBase = nullptr;
		CircleVertex* CircleVertexBufferPtr = nullptr;

		uint32_t LineVertexCount = 0;
		LineVertex* LineVertexBufferBase = nullptr;
		LineVertex* LineVertexBufferPtr = nullptr;

		float LineWidth = 2.0f;

		std::array<Ref<Texture2D>, MaxTextureSlots> TextureSlots;
		uint32_t TextureSlotIndex = 1; // 0 = white texture
		glm::vec4 QuadVertexPositions[4];
		Renderer2D::Statistics Stats;

		struct CameraData
		{
			glm::mat4 ViewProjection;
		};
		CameraData CameraBuffer;

		// ---- Unified RHI resources (one set shared by all backends) ----
		Ref<RHIBuffer>           QuadVB, QuadIB, CircleVB, LineVB;
		Ref<RHIBuffer>           CameraCB; // constant buffer for ViewProjection
		Ref<RHIGraphicsPipeline> QuadPipeline, CirclePipeline, LinePipeline;

		// Backend activation flags (exactly one is true; selects the Init branch)
		bool D3D12Active = false;
		bool VkActive    = false;
		bool OL_Active   = false;

		// Active render target for D3D12/OpenGL/Vulkan flushing (RHI-bridged).
		Ref<RHIFramebuffer> ActiveRenderTarget;
		// True when the active render target has not been cleared since it was
		// (re-)bound via SetActiveRenderTarget. The first Flush after binding uses
		// LoadOp::Clear; subsequent Flushes in the same frame (e.g. the editor's
		// overlay pass) must use LoadOp::Load so they don't erase what earlier
		// passes already drew into the same target.
		bool ActiveRenderTargetPendingClear = true;

		// ---- Vulkan-only init data ([FROZEN] backend) ----
		VulkanDevice* VkDev = nullptr;
		VkDescriptorSetLayout VkDescLayout = VK_NULL_HANDLE;
		VkDescriptorSet       VkDescSet    = VK_NULL_HANDLE;
		VkDescriptorPool      VkDescPool   = VK_NULL_HANDLE;
	};

	static Renderer2DData s_Data;

	void Renderer2D::Init()
	{
		s_Data.D3D12Active = (Renderer::GetAPI() == RendererAPI::API::D3D12);
		s_Data.VkActive    = (Renderer::GetAPI() == RendererAPI::API::Vulkan);
		s_Data.OL_Active   = (Renderer::GetAPI() == RendererAPI::API::OpenGL);

		if (s_Data.VkActive)
		{
			CANDY_CORE_INFO("Renderer2D: initializing Vulkan backend (triangle SPIR-V shaders)...");

			auto* gfxCtx = dynamic_cast<VulkanGraphicsContext*>(
				Application::Get().GetWindow().GetGraphicsContext());
			if (!gfxCtx) { CANDY_CORE_ERROR("Renderer2D: Vulkan API but no VulkanGraphicsContext"); return; }
			s_Data.VkDev = gfxCtx->GetDevice();

			auto* dev = s_Data.VkDev;
			VkDevice vkDev = dev->GetVkDevice();

			// --- CPU-side vertex buffers ---
			s_Data.QuadVertexBufferBase   = new QuadVertex[s_Data.MaxVertices];
			s_Data.CircleVertexBufferBase = new CircleVertex[s_Data.MaxVertices];
			s_Data.LineVertexBufferBase   = new LineVertex[s_Data.MaxVertices];

			// --- GPU vertex buffers (upload heap, CPU-accessible) ---
			auto makeUploadVB = [&](uint64_t size, uint32_t stride, const char* name) -> Ref<RHIBuffer> {
				BufferDesc d; d.Size=size; d.Usage=ResourceUsage::VertexBuffer; d.CPUAccessible=true; d.Stride=stride; d.DebugName=name;
				return dev->CreateBuffer(d);
			};
			s_Data.QuadVB   = makeUploadVB(s_Data.MaxVertices * sizeof(QuadVertex),  sizeof(QuadVertex),  "Vk2D_QuadVB");
			s_Data.CircleVB = makeUploadVB(s_Data.MaxVertices * sizeof(CircleVertex),sizeof(CircleVertex),"Vk2D_CircleVB");
			s_Data.LineVB   = makeUploadVB(s_Data.MaxVertices * sizeof(LineVertex),  sizeof(LineVertex),  "Vk2D_LineVB");

			// Index buffer (with data uploaded)
			{
				uint32_t* indices = new uint32_t[s_Data.MaxIndices];
				uint32_t off = 0;
				for (uint32_t i = 0; i < s_Data.MaxIndices; i += 6)
				{ indices[i+0]=off; indices[i+1]=off+1; indices[i+2]=off+2; indices[i+3]=off+2; indices[i+4]=off+3; indices[i+5]=off; off+=4; }

				// Create host-visible index buffer and upload data
				{
					BufferDesc ibDesc;
					ibDesc.Size = s_Data.MaxIndices * sizeof(uint32_t);
					ibDesc.Usage = ResourceUsage::IndexBuffer;
					ibDesc.CPUAccessible = true; // host-visible for upload
					ibDesc.DebugName = "Vk2D_QuadIB";
					s_Data.QuadIB = dev->CreateBuffer(ibDesc);
				}
				// Upload
				s_Data.QuadIB->Write(indices, s_Data.MaxIndices * sizeof(uint32_t));
				delete[] indices;
			}

			// Camera CB
			{
				BufferDesc d; d.Size=256; d.Usage=ResourceUsage::ConstantBuffer; d.CPUAccessible=true; d.DebugName="Vk2D_CameraCB";
				s_Data.CameraCB = dev->CreateBuffer(d);
			}

			// --- Shaders (use built-in triangle SPIR-V) ---
			auto& vsSpv = dev->GetTriangleVSSPIRV();
			auto& psSpv = dev->GetTrianglePSSPIRV();
			auto quadVS = dev->CreateShaderModule(vsSpv.data(), static_cast<uint32_t>(vsSpv.size()*4), "QuadVS");
			auto quadPS = dev->CreateShaderModule(psSpv.data(), static_cast<uint32_t>(psSpv.size()*4), "QuadPS");
			auto circleVS = quadVS; auto circlePS = quadPS;
			auto lineVS   = quadVS; auto linePS   = quadPS;

			// --- Descriptor set layout + pool + set ---
			{
				VkDescriptorSetLayoutBinding uboBinding = {};
				uboBinding.binding         = 0;
				uboBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				uboBinding.descriptorCount = 1;
				uboBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

				VkDescriptorSetLayoutCreateInfo dsli = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
				dsli.bindingCount = 1;
				dsli.pBindings    = &uboBinding;
				dev->fnCreateDescriptorSetLayout(vkDev, &dsli, nullptr, &s_Data.VkDescLayout);

				VkDescriptorPoolSize poolSize = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 };
				VkDescriptorPoolCreateInfo dpci = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
				dpci.poolSizeCount = 1;
				dpci.pPoolSizes    = &poolSize;
				dpci.maxSets       = 1;
				dev->fnCreateDescriptorPool(vkDev, &dpci, nullptr, &s_Data.VkDescPool);

				VkDescriptorSetAllocateInfo dsai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
				dsai.descriptorPool     = s_Data.VkDescPool;
				dsai.descriptorSetCount = 1;
				dsai.pSetLayouts        = &s_Data.VkDescLayout;
				dev->fnAllocateDescriptorSets(vkDev, &dsai, &s_Data.VkDescSet);
			}

			// --- Pipelines ---
			{
				GraphicsPipelineDesc pd;
				pd.Topology=PrimitiveTopology::Triangles; pd.Rasterizer.Cull=CullMode::None; pd.Rasterizer.Fill=FillMode::Solid;
				pd.DepthStencil.DepthTestEnable=false; pd.DepthStencil.DepthWriteEnable=false;
				pd.Blend.BlendEnable=true; pd.Blend.SrcColorBlendFactor=BlendState::BlendFactor::SrcAlpha;
				pd.Blend.DstColorBlendFactor=BlendState::BlendFactor::OneMinusSrcAlpha; pd.Blend.WriteMask=ColorWriteMask::All;
				pd.RenderTargetFormats={RHIFormat::R8G8B8A8Unorm};

				// Quad
				{ GraphicsPipelineDesc qd=pd; VertexInputLayout::VertexBinding b; b.Binding=0; b.Stride=sizeof(QuadVertex);
				  qd.VertexInput.Bindings.push_back(b);
				  qd.VertexInput.Attributes.push_back({0,0,RHIFormat::R32G32B32Float,0});
				  qd.VertexInput.Attributes.push_back({1,0,RHIFormat::R32G32B32A32Float,offsetof(QuadVertex,Color)});
				  s_Data.QuadPipeline=dev->CreateGraphicsPipeline(qd,quadVS,quadPS); }

				// Circle
				{ GraphicsPipelineDesc cd=pd; VertexInputLayout::VertexBinding b; b.Binding=0; b.Stride=sizeof(CircleVertex);
				  cd.VertexInput.Bindings.push_back(b);
				  cd.VertexInput.Attributes.push_back({0,0,RHIFormat::R32G32B32Float,0});
				  cd.VertexInput.Attributes.push_back({2,0,RHIFormat::R32G32B32A32Float,offsetof(CircleVertex,Color)});
				  s_Data.CirclePipeline=dev->CreateGraphicsPipeline(cd,circleVS,circlePS); }

				// Line
				{ GraphicsPipelineDesc ld=pd; ld.Topology=PrimitiveTopology::Lines; VertexInputLayout::VertexBinding b; b.Binding=0; b.Stride=sizeof(LineVertex);
				  ld.VertexInput.Bindings.push_back(b);
				  ld.VertexInput.Attributes.push_back({0,0,RHIFormat::R32G32B32Float,0});
				  ld.VertexInput.Attributes.push_back({1,0,RHIFormat::R32G32B32A32Float,offsetof(LineVertex,Color)});
				  s_Data.LinePipeline=dev->CreateGraphicsPipeline(ld,lineVS,linePS); }
			}

			s_Data.WhiteTexture = Texture2D::Create(1, 1);
			uint32_t wtd=0xffffffff; s_Data.WhiteTexture->SetData(&wtd,sizeof(uint32_t));
			s_Data.TextureSlots[0]=s_Data.WhiteTexture;
			s_Data.QuadVertexPositions[0]={-0.5f,-0.5f,0.0f,1.0f}; s_Data.QuadVertexPositions[1]={0.5f,-0.5f,0.0f,1.0f};
			s_Data.QuadVertexPositions[2]={0.5f,0.5f,0.0f,1.0f}; s_Data.QuadVertexPositions[3]={-0.5f,0.5f,0.0f,1.0f};

			CANDY_CORE_INFO("Renderer2D: Vulkan backend initialized (colored primitives only)");
			return;
		}

	// =====================================================
	// Unified D3D12 / OpenGL init (Vulkan branch above is [FROZEN])
	// =====================================================
	{
		auto* dev = RHIContext::GetDevice();
		if (!dev)
		{
			CANDY_CORE_ERROR("Renderer2D: no RHI device published");
			return;
		}

		// --- CPU-side vertex buffers (batched CPU writes each frame) ---
		s_Data.QuadVertexBufferBase   = new QuadVertex[s_Data.MaxVertices];
		s_Data.CircleVertexBufferBase = new CircleVertex[s_Data.MaxVertices];
		s_Data.LineVertexBufferBase   = new LineVertex[s_Data.MaxVertices];

		// --- GPU upload-heap vertex buffers ---
		auto makeUploadVB = [&](uint64_t size, uint32_t stride, const char* name) -> Ref<RHIBuffer> {
			BufferDesc d;
			d.Size          = size;
			d.Usage         = ResourceUsage::VertexBuffer;
			d.CPUAccessible = true;
			d.Stride        = stride;
			d.DebugName     = name;
			return dev->CreateBuffer(d);
		};
		s_Data.QuadVB   = makeUploadVB(s_Data.MaxVertices * sizeof(QuadVertex),   sizeof(QuadVertex),   "Renderer2D_QuadVB");
		s_Data.CircleVB = makeUploadVB(s_Data.MaxVertices * sizeof(CircleVertex), sizeof(CircleVertex), "Renderer2D_CircleVB");
		s_Data.LineVB   = makeUploadVB(s_Data.MaxVertices * sizeof(LineVertex),   sizeof(LineVertex),   "Renderer2D_LineVB");

		// --- Index buffer (quad/circle share) ---
		{
			uint32_t* quadIndices = new uint32_t[s_Data.MaxIndices];
			uint32_t offset = 0;
			for (uint32_t i = 0; i < s_Data.MaxIndices; i += 6)
			{
				quadIndices[i + 0] = offset + 0;
				quadIndices[i + 1] = offset + 1;
				quadIndices[i + 2] = offset + 2;
				quadIndices[i + 3] = offset + 2;
				quadIndices[i + 4] = offset + 3;
				quadIndices[i + 5] = offset + 0;
				offset += 4;
			}
			BufferDesc ib;
			ib.Size          = s_Data.MaxIndices * sizeof(uint32_t);
			ib.Usage         = ResourceUsage::IndexBuffer;
			ib.CPUAccessible = true;
			ib.DebugName     = "Renderer2D_QuadIB";
			s_Data.QuadIB = dev->CreateBuffer(ib);
			s_Data.QuadIB->Write(quadIndices, static_cast<size_t>(ib.Size));
			delete[] quadIndices;
		}

		// --- Camera constant buffer ---
		{
			BufferDesc cb;
			cb.Size          = 256; // 256-byte aligned (D3D12 CBV requirement)
			cb.Usage         = ResourceUsage::ConstantBuffer;
			cb.CPUAccessible = true;
			cb.DebugName     = "Renderer2D_CameraCB";
			s_Data.CameraCB = dev->CreateBuffer(cb);
		}

		// --- Shader modules: per-API source directory; one multi-stage file per primitive ---
		const char* shaderDir = s_Data.D3D12Active ? "DX12" : "OpenGL";
		const char* shaderExt = s_Data.D3D12Active ? "hlsl" : "glsl";
		auto loadModule = [&](const char* file, ShaderStage stage, const char* entry) -> Ref<RHIShaderModule> {
			std::string path = std::string("VFS://Engine/Shaders/") + shaderDir + "/" + file + "." + shaderExt;
			auto src = FileSystem::Get().ReadText(path);
			if (!src)
			{
				CANDY_CORE_ERROR("Renderer2D: failed to load shader '{}'", path);
				return nullptr;
			}
			return dev->CreateShaderModuleFromSource(src->c_str(), stage, entry, file);
		};
		// OpenGL ignores stage/entry and returns a whole-file module; the same
		// module is passed as both vs and fs and split at pipeline creation.
		Ref<RHIShaderModule> quadVS   = loadModule("Renderer2D_Quad",   ShaderStage::Vertex,   "VSMain");
		Ref<RHIShaderModule> quadPS   = loadModule("Renderer2D_Quad",   ShaderStage::Fragment, "PSMain");
		Ref<RHIShaderModule> circleVS = loadModule("Renderer2D_Circle", ShaderStage::Vertex,   "VSMain");
		Ref<RHIShaderModule> circlePS = loadModule("Renderer2D_Circle", ShaderStage::Fragment, "PSMain");
		Ref<RHIShaderModule> lineVS   = loadModule("Renderer2D_Line",   ShaderStage::Vertex,   "VSMain");
		Ref<RHIShaderModule> linePS   = loadModule("Renderer2D_Line",   ShaderStage::Fragment, "PSMain");

		// --- Pipelines ---
		GraphicsPipelineDesc base;
		base.Topology                  = PrimitiveTopology::Triangles;
		base.Rasterizer.Cull           = CullMode::None;
		base.Rasterizer.Fill           = FillMode::Solid;
		base.DepthStencil.DepthTestEnable  = false;
		base.DepthStencil.DepthWriteEnable = false;
		base.DepthStencilFormat        = RHIFormat::D24UnormS8Uint; // viewport framebuffer has D24S8 depth
		base.Blend.BlendEnable         = true;
		base.Blend.SrcColorBlendFactor = BlendState::BlendFactor::SrcAlpha;
		base.Blend.DstColorBlendFactor = BlendState::BlendFactor::OneMinusSrcAlpha;
		base.Blend.WriteMask           = ColorWriteMask::All;
		base.RenderTargetFormats       = { RHIFormat::R8G8B8A8Unorm, RHIFormat::R32Sint };

		// Quad
		{
			GraphicsPipelineDesc pd = base;
			VertexInputLayout::VertexBinding b; b.Binding = 0; b.Stride = sizeof(QuadVertex);
			pd.VertexInput.Bindings.push_back(b);
			pd.VertexInput.Attributes.push_back({ 0, 0, RHIFormat::R32G32B32Float,    0 });
			pd.VertexInput.Attributes.push_back({ 1, 0, RHIFormat::R32G32B32A32Float, offsetof(QuadVertex, Color) });
			pd.VertexInput.Attributes.push_back({ 2, 0, RHIFormat::R32G32Float,       offsetof(QuadVertex, TexCoord) });
			pd.VertexInput.Attributes.push_back({ 3, 0, RHIFormat::R32Float,          offsetof(QuadVertex, TexIndex) });
			pd.VertexInput.Attributes.push_back({ 4, 0, RHIFormat::R32Float,          offsetof(QuadVertex, TilingFactor) });
			pd.VertexInput.Attributes.push_back({ 5, 0, RHIFormat::R32Sint,           offsetof(QuadVertex, EntityID) });
			s_Data.QuadPipeline = dev->CreateGraphicsPipeline(pd, quadVS, quadPS);
		}
		// Circle
		{
			GraphicsPipelineDesc pd = base;
			VertexInputLayout::VertexBinding b; b.Binding = 0; b.Stride = sizeof(CircleVertex);
			pd.VertexInput.Bindings.push_back(b);
			pd.VertexInput.Attributes.push_back({ 0, 0, RHIFormat::R32G32B32Float,    0 });
			pd.VertexInput.Attributes.push_back({ 1, 0, RHIFormat::R32G32B32Float,   offsetof(CircleVertex, LocalPosition) });
			pd.VertexInput.Attributes.push_back({ 2, 0, RHIFormat::R32G32B32A32Float, offsetof(CircleVertex, Color) });
			pd.VertexInput.Attributes.push_back({ 3, 0, RHIFormat::R32Float,         offsetof(CircleVertex, Thickness) });
			pd.VertexInput.Attributes.push_back({ 4, 0, RHIFormat::R32Float,         offsetof(CircleVertex, Fade) });
			pd.VertexInput.Attributes.push_back({ 5, 0, RHIFormat::R32Sint,           offsetof(CircleVertex, EntityID) });
			s_Data.CirclePipeline = dev->CreateGraphicsPipeline(pd, circleVS, circlePS);
		}
		// Line
		{
			GraphicsPipelineDesc pd = base;
			pd.Topology = PrimitiveTopology::Lines;
			VertexInputLayout::VertexBinding b; b.Binding = 0; b.Stride = sizeof(LineVertex);
			pd.VertexInput.Bindings.push_back(b);
			pd.VertexInput.Attributes.push_back({ 0, 0, RHIFormat::R32G32B32Float,    0 });
			pd.VertexInput.Attributes.push_back({ 1, 0, RHIFormat::R32G32B32A32Float, offsetof(LineVertex, Color) });
			pd.VertexInput.Attributes.push_back({ 2, 0, RHIFormat::R32Sint,           offsetof(LineVertex, EntityID) });
			s_Data.LinePipeline = dev->CreateGraphicsPipeline(pd, lineVS, linePS);
		}

		// --- White texture + quad corner positions ---
		s_Data.WhiteTexture = Texture2D::Create(1, 1);
		uint32_t whiteTextureData = 0xffffffff;
		s_Data.WhiteTexture->SetData(&whiteTextureData, sizeof(uint32_t));
		s_Data.TextureSlots[0] = s_Data.WhiteTexture;

		s_Data.QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[2] = {  0.5f,  0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

		CANDY_CORE_INFO("Renderer2D: {} backend initialized", RendererAPI::StringFromAPI(Renderer::GetAPI()));
	}
	}

	void Renderer2D::Shutdown()
	{
		delete[] s_Data.QuadVertexBufferBase;
		delete[] s_Data.CircleVertexBufferBase;
		delete[] s_Data.LineVertexBufferBase;

		s_Data.QuadVB.reset();
		s_Data.QuadIB.reset();
		s_Data.CircleVB.reset();
		s_Data.LineVB.reset();
		s_Data.CameraCB.reset();
		s_Data.QuadPipeline.reset();
		s_Data.CirclePipeline.reset();
		s_Data.LinePipeline.reset();

		if (s_Data.VkActive && s_Data.VkDev)
		{
			VkDevice vd = s_Data.VkDev->GetVkDevice();
			if (s_Data.VkDescSet)  s_Data.VkDev->fnFreeCommandBuffers(vd, VK_NULL_HANDLE, 0, nullptr); // pool auto-frees sets
			if (s_Data.VkDescPool) s_Data.VkDev->fnDestroyDescriptorPool(vd, s_Data.VkDescPool, nullptr);
			if (s_Data.VkDescLayout) s_Data.VkDev->fnDestroyDescriptorSetLayout(vd, s_Data.VkDescLayout, nullptr);
		}
	}

		void Renderer2D::BeginScene(const OrthographicCamera& camera)
	{
		// TODO: legacy 2D ortho path �� camera matrix currently unused by the RHI
		// path (no callers render through this overload on RHI backends).
		StartBatch();
	}

	void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		s_Data.CameraBuffer.ViewProjection = camera.GetProjection() * glm::inverse(transform);
		StartBatch();
	}

	void Renderer2D::BeginScene(const EditorCamera& camera)
	{
		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjection();
		StartBatch();
	}

	void Renderer2D::EndScene()
	{
		Flush();
	}

	void Renderer2D::StartBatch()
	{
		s_Data.QuadIndexCount = 0;
		s_Data.QuadVertexBufferPtr = s_Data.QuadVertexBufferBase;

		s_Data.CircleIndexCount = 0;
		s_Data.CircleVertexBufferPtr = s_Data.CircleVertexBufferBase;

		s_Data.LineVertexCount = 0;
		s_Data.LineVertexBufferPtr = s_Data.LineVertexBufferBase;

		s_Data.TextureSlotIndex = 1;
	}
	void Renderer2D::Flush()
	{
		static bool s_FirstFlush = true;
		if (s_FirstFlush)
		{
			s_FirstFlush = false;
			CANDY_CORE_INFO("Renderer2D::Flush FIRST CALL — API={}, activeRT={} QuadIdx={} CircleIdx={} LineVtx={}",
			                RendererAPI::StringFromAPI(Renderer::GetAPI()),
			                (bool)s_Data.ActiveRenderTarget,
			                s_Data.QuadIndexCount, s_Data.CircleIndexCount, s_Data.LineVertexCount);
		}

		auto* dev = RHIContext::GetDevice();
		if (!dev)
		{
			CANDY_CORE_ERROR("Renderer2D::Flush — no RHI device published");
			return;
		}

		// Upload camera constant buffer
		if (!s_Data.CameraCB->Write(&s_Data.CameraBuffer, sizeof(s_Data.CameraBuffer)))
		{
			CANDY_CORE_ERROR("Renderer2D::Flush — CameraCB upload failed; skipping this frame");
			return;
		}

		auto& queue = dev->GetCommandQueue();
		auto  cmd   = queue.CreateCommandBuffer();
		if (!cmd) return;

		cmd->Begin();

		// Clear only on the first Flush after the target was bound; later passes
		// in the same frame (overlay, camera preview restore, ...) load instead,
		// so an empty overlay pass can't erase the scene that was just rendered.
		// Swap-chain rendering (no active target) always clears.
		const LoadOp loadOp = (!s_Data.ActiveRenderTarget || s_Data.ActiveRenderTargetPendingClear)
			? LoadOp::Clear : LoadOp::Load;

		RenderPassDesc rpDesc;
		{
			RenderPassColorAttachment colorAttachment;
			colorAttachment.Format = RHIFormat::R8G8B8A8Unorm;
			colorAttachment.LoadOp = loadOp;
			colorAttachment.ClearColor[0] = 0.1f;
			colorAttachment.ClearColor[1] = 0.1f;
			colorAttachment.ClearColor[2] = 0.1f;
			colorAttachment.ClearColor[3] = 1.0f;
			rpDesc.ColorAttachments.push_back(colorAttachment);

			// Entity ID attachment
			if (s_Data.ActiveRenderTarget && s_Data.ActiveRenderTarget->GetColorAttachmentCount() > 1)
			{
				RenderPassColorAttachment idAttachment;
				idAttachment.Format = RHIFormat::R32Sint;
				idAttachment.LoadOp = loadOp;
				idAttachment.ClearColor[0] = -1.0f;
				idAttachment.ClearColor[1] = -1.0f;
				idAttachment.ClearColor[2] = -1.0f;
				idAttachment.ClearColor[3] = -1.0f;
				rpDesc.ColorAttachments.push_back(idAttachment);
			}
		}
		cmd->BeginRenderPass(s_Data.ActiveRenderTarget.get(), rpDesc);
		s_Data.ActiveRenderTargetPendingClear = false;

		// Viewport + scissor follow the render target
		uint32_t vpW = 1280, vpH = 720;
		if (s_Data.ActiveRenderTarget)
		{
			vpW = s_Data.ActiveRenderTarget->GetWidth();
			vpH = s_Data.ActiveRenderTarget->GetHeight();
		}
		cmd->SetViewport(0, 0, static_cast<float>(vpW), static_cast<float>(vpH));
		cmd->SetScissor(0, 0, vpW, vpH);

		// --- Quad batch (textured) ---
		if (s_Data.QuadIndexCount && s_Data.QuadPipeline)
		{
			uint32_t dataSize = static_cast<uint32_t>(
				reinterpret_cast<uint8_t*>(s_Data.QuadVertexBufferPtr) -
				reinterpret_cast<uint8_t*>(s_Data.QuadVertexBufferBase));

			if (!s_Data.QuadVB->Write(s_Data.QuadVertexBufferBase, dataSize))
				CANDY_CORE_WARN("Renderer2D::Flush — QuadVB upload failed; stale data used");

			cmd->SetPipeline(s_Data.QuadPipeline);
			cmd->SetConstantBuffer(0, 0, s_Data.CameraCB);
			cmd->SetVertexBuffer(s_Data.QuadVB);
			cmd->SetIndexBuffer(s_Data.QuadIB);

			// Bind used textures; unfilled slots fall back to the white texture.
			for (uint32_t i = 0; i < s_Data.MaxTextureSlots; ++i)
			{
				Ref<Texture2D> src = (i < s_Data.TextureSlotIndex) ? s_Data.TextureSlots[i] : s_Data.TextureSlots[0];
				if (src)
				{
					if (auto rhiTex = src->GetRHITexture())
						cmd->SetTexture(1, i, rhiTex);
				}
			}

			cmd->DrawIndexed(s_Data.QuadIndexCount);
			s_Data.Stats.DrawCalls++;
		}

		// --- Circle batch ---
		if (s_Data.CircleIndexCount && s_Data.CirclePipeline)
		{
			uint32_t dataSize = static_cast<uint32_t>(
				reinterpret_cast<uint8_t*>(s_Data.CircleVertexBufferPtr) -
				reinterpret_cast<uint8_t*>(s_Data.CircleVertexBufferBase));

			if (!s_Data.CircleVB->Write(s_Data.CircleVertexBufferBase, dataSize))
				CANDY_CORE_WARN("Renderer2D::Flush — CircleVB upload failed; stale data used");

			cmd->SetPipeline(s_Data.CirclePipeline);
			cmd->SetConstantBuffer(0, 0, s_Data.CameraCB);
			cmd->SetVertexBuffer(s_Data.CircleVB);
			cmd->SetIndexBuffer(s_Data.QuadIB); // reuse quad IB
			cmd->DrawIndexed(s_Data.CircleIndexCount);
			s_Data.Stats.DrawCalls++;
		}

		// --- Line batch ---
		if (s_Data.LineVertexCount && s_Data.LinePipeline)
		{
			uint32_t dataSize = static_cast<uint32_t>(
				reinterpret_cast<uint8_t*>(s_Data.LineVertexBufferPtr) -
				reinterpret_cast<uint8_t*>(s_Data.LineVertexBufferBase));

			if (!s_Data.LineVB->Write(s_Data.LineVertexBufferBase, dataSize))
				CANDY_CORE_WARN("Renderer2D::Flush — LineVB upload failed; stale data used");

			cmd->SetPipeline(s_Data.LinePipeline);
			cmd->SetConstantBuffer(0, 0, s_Data.CameraCB);
			cmd->SetVertexBuffer(s_Data.LineVB);
			cmd->Draw(s_Data.LineVertexCount);
			s_Data.Stats.DrawCalls++;
		}

		cmd->EndRenderPass();
		cmd->End();

		queue.Submit({ cmd.get() });
		// No Present when targeting a framebuffer
		dev->WaitIdle();
	}

	void Renderer2D::NextBatch()
	{
		Flush();
		StartBatch();
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		DrawQuad(transform, color);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		DrawQuad(transform, texture, tilingFactor, tintColor); // error DrawQuad(transform, texture, tintColor);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityID)
	{
		const size_t quadVertexCount = 4;

		const float textureIndex = 0.0f; // White Texture
		const float tilingFactor = 1.0f;
		const glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
		
		if (s_Data.QuadIndexCount >= Renderer2DData::MaxIndices)
			NextBatch();

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
			s_Data.QuadVertexBufferPtr->Color = color;
			s_Data.QuadVertexBufferPtr->TexCoord = textureCoords[i];
			s_Data.QuadVertexBufferPtr->TexIndex = textureIndex;
			s_Data.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_Data.QuadVertexBufferPtr->EntityID = entityID;
			s_Data.QuadVertexBufferPtr++;
		}

		s_Data.QuadIndexCount += 6;

		s_Data.Stats.QuadCount++;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor, int entityID)
	{
		constexpr size_t quadVertexCount = 4;
		const glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

		if (s_Data.QuadIndexCount >= Renderer2DData::MaxIndices)
			NextBatch();

		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < s_Data.TextureSlotIndex; i++)
		{
			if (*s_Data.TextureSlots[i] == *texture)
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			if (s_Data.TextureSlotIndex >= Renderer2DData::MaxTextureSlots)
				NextBatch();

			textureIndex = (float)s_Data.TextureSlotIndex;
			s_Data.TextureSlots[s_Data.TextureSlotIndex] = texture;
			s_Data.TextureSlotIndex++;
		}

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
			s_Data.QuadVertexBufferPtr->Color = tintColor;
			s_Data.QuadVertexBufferPtr->TexCoord = textureCoords[i];
			s_Data.QuadVertexBufferPtr->TexIndex = textureIndex;
			s_Data.QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_Data.QuadVertexBufferPtr->EntityID = entityID;
			s_Data.QuadVertexBufferPtr++;
		}

		s_Data.QuadIndexCount += 6;

		s_Data.Stats.QuadCount++;
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, color);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), glm::radians(rotation), { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		DrawQuad(transform, color);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), glm::radians(rotation), { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		DrawQuad(transform, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness /*= 1.0f*/, float fade /*= 0.005f*/, int entityID /*= -1*/)
	{
		// TODO: implement for circles
		// if (s_Data.QuadIndexCount >= Renderer2DData::MaxIndices)
		// 	NextBatch();

		for (size_t i = 0; i < 4; i++)
		{
			s_Data.CircleVertexBufferPtr->WorldPosition = transform * s_Data.QuadVertexPositions[i];
			s_Data.CircleVertexBufferPtr->LocalPosition = s_Data.QuadVertexPositions[i] * 2.0f;
			s_Data.CircleVertexBufferPtr->Color = color;
			s_Data.CircleVertexBufferPtr->Thickness = thickness;
			s_Data.CircleVertexBufferPtr->Fade = fade;
			s_Data.CircleVertexBufferPtr->EntityID = entityID;
			s_Data.CircleVertexBufferPtr++;
		}

		s_Data.CircleIndexCount += 6;

		s_Data.Stats.QuadCount++;
	}

	void Renderer2D::DrawLine(const glm::vec3& p0, glm::vec3& p1, const glm::vec4& color, int entityID)
	{
		s_Data.LineVertexBufferPtr->Position = p0;
		s_Data.LineVertexBufferPtr->Color = color;
		s_Data.LineVertexBufferPtr->EntityID = entityID;
		s_Data.LineVertexBufferPtr++;

		s_Data.LineVertexBufferPtr->Position = p1;
		s_Data.LineVertexBufferPtr->Color = color;
		s_Data.LineVertexBufferPtr->EntityID = entityID;
		s_Data.LineVertexBufferPtr++;

		s_Data.LineVertexCount += 2;
	}

	void Renderer2D::DrawRect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, int entityID)
	{
		glm::vec3 p0 = glm::vec3(position.x - size.x * 0.5f, position.y - size.y * 0.5f, position.z);
		glm::vec3 p1 = glm::vec3(position.x + size.x * 0.5f, position.y - size.y * 0.5f, position.z);
		glm::vec3 p2 = glm::vec3(position.x + size.x * 0.5f, position.y + size.y * 0.5f, position.z);
		glm::vec3 p3 = glm::vec3(position.x - size.x * 0.5f, position.y + size.y * 0.5f, position.z);

		DrawLine(p0, p1, color);
		DrawLine(p1, p2, color);
		DrawLine(p2, p3, color);
		DrawLine(p3, p0, color);
	}

	void Renderer2D::DrawRect(const glm::mat4& transform, const glm::vec4& color, int entityID)
	{
		glm::vec3 lineVertices[4];
		for (size_t i = 0; i < 4; i++)
			lineVertices[i] = transform * s_Data.QuadVertexPositions[i];

		DrawLine(lineVertices[0], lineVertices[1], color);
		DrawLine(lineVertices[1], lineVertices[2], color);
		DrawLine(lineVertices[2], lineVertices[3], color);
		DrawLine(lineVertices[3], lineVertices[0], color);
	}

	void Renderer2D::DrawSprite(const glm::mat4& transform, SpriteRendererComponent& src, int entityID)
	{
		if (src.Texture)
			DrawQuad(transform, src.Texture, src.TilingFactor, src.Color, entityID);
		else
			DrawQuad(transform, src.Color, entityID);
	}

	float Renderer2D::GetLineWidth()
	{
		return s_Data.LineWidth;
	}

	void Renderer2D::SetLineWidth(float width)
	{
		s_Data.LineWidth = width;
	}

	void Renderer2D::ResetStats()
	{
		memset(&s_Data.Stats, 0, sizeof(Statistics));
	}

	Renderer2D::Statistics Renderer2D::GetStats()
	{
		return s_Data.Stats;
	}

	void Renderer2D::SetActiveRenderTarget(const Ref<RHIFramebuffer>& fb)
	{
		s_Data.ActiveRenderTarget = fb;
		// (Re-)binding a render target marks it as needing a clear on the next
		// Flush. This also re-arms the clear at the start of every frame, since
		// callers re-bind the viewport framebuffer each frame.
		s_Data.ActiveRenderTargetPendingClear = true;
	}
}

