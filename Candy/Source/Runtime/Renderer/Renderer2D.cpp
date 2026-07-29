#include "CandyPCH.h"

#include "Runtime/Renderer/Shader.h"
#include "Runtime/Renderer/Renderer2D.h"
#include "Runtime/Renderer/VertexArray.h"
#include "Runtime/Renderer/UniformBuffer.h"
#include "Runtime/Renderer/RenderCommand.h"
#include "Runtime/Renderer/Renderer.h"
#include "Runtime/Renderer/GraphicsContext.h"
#include "Runtime/Core/FileSystem.h"
#include "Runtime/Core/Application.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// DX12 backend includes
#include "Platform/DX12/DX12Device.h"
#include "Platform/DX12/DX12GraphicsContext.h"
#include "Platform/DX12/DX12Buffer.h"
#include "Platform/DX12/DX12PipelineState.h"
#include "Platform/DX12/DX12Framebuffer.h"
#include "Platform/DX12/DX12CommandBuffer.h"
#include "Platform/DX12/DX12Texture2D.h"
#include "Platform/DX12/DX12Texture.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanGraphicsContext.h"
#include "Platform/Vulkan/VulkanBuffer.h"
#include "Platform/Vulkan/VulkanPipelineState.h"


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

		Ref<VertexArray> QuadVertexArray;
		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<Shader> QuadShader;
		Ref<Texture2D> WhiteTexture;

		Ref<VertexArray> CircleVertexArray;
		Ref<VertexBuffer> CircleVertexBuffer;
		Ref<Shader> CircleShader;

		Ref<VertexArray> LineVertexArray;
		Ref<VertexBuffer> LineVertexBuffer;
		Ref<Shader> LineShader;

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
		Ref<UniformBuffer> CameraUniformBuffer;

		// ---- DX12 backend data ----
		bool DX12Active = false;
		DX12Device* DX12Dev = nullptr;

		// Vertex buffers (upload heap)
		Ref<RHIBuffer> DX12QuadVB;
		Ref<RHIBuffer> DX12QuadIB;
		Ref<RHIBuffer> DX12CircleVB;
		Ref<RHIBuffer> DX12LineVB;
		Ref<RHIBuffer> DX12CameraCB; // constant buffer for ViewProjection

		// Pipelines
		Ref<RHIGraphicsPipeline> DX12QuadPipeline;
		Ref<RHIGraphicsPipeline> DX12CirclePipeline;
		Ref<RHIGraphicsPipeline> DX12LinePipeline;

		// Shader modules
		Ref<RHIShaderModule> DX12QuadVS, DX12QuadPS;
		Ref<RHIShaderModule> DX12CircleVS, DX12CirclePS;
		Ref<RHIShaderModule> DX12LineVS, DX12LinePS;

		// Textured root signature (shared by quad pipeline)
		Microsoft::WRL::ComPtr<ID3D12RootSignature> DX12TexturedRootSig;

		// Active render target for DX12 flushing
		DX12Framebuffer* DX12ActiveFramebuffer = nullptr;

		// ---- Vulkan backend data ----
		bool VkActive = false;
		VulkanDevice* VkDev = nullptr;

		// Vulkan vertex/index buffers
		Ref<RHIBuffer> VkQuadVB, VkQuadIB, VkCircleVB, VkLineVB, VkCameraCB;

		// Vulkan pipelines
		Ref<RHIGraphicsPipeline> VkQuadPipeline, VkCirclePipeline, VkLinePipeline;
		Ref<RHIShaderModule>     VkQuadVS, VkQuadPS, VkCircleVS, VkCirclePS, VkLineVS, VkLinePS;

		// Descriptor set
		VkDescriptorSetLayout VkDescLayout = VK_NULL_HANDLE;
		VkDescriptorSet       VkDescSet    = VK_NULL_HANDLE;
		VkDescriptorPool      VkDescPool   = VK_NULL_HANDLE;
	};

	static Renderer2DData s_Data;

	void Renderer2D::Init()
	{
		s_Data.DX12Active = (Renderer::GetAPI() == RendererAPI::API::DX12);
		s_Data.VkActive    = (Renderer::GetAPI() == RendererAPI::API::Vulkan);

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
			auto makeUploadVB = [&](uint64_t size, const char* name) -> Ref<RHIBuffer> {
				BufferDesc d; d.Size=size; d.Usage=ResourceUsage::VertexBuffer; d.CPUAccessible=true; d.DebugName=name;
				return dev->CreateBuffer(d);
			};
			s_Data.VkQuadVB   = makeUploadVB(s_Data.MaxVertices * sizeof(QuadVertex),  "Vk2D_QuadVB");
			s_Data.VkCircleVB = makeUploadVB(s_Data.MaxVertices * sizeof(CircleVertex),"Vk2D_CircleVB");
			s_Data.VkLineVB   = makeUploadVB(s_Data.MaxVertices * sizeof(LineVertex),  "Vk2D_LineVB");

			// Index buffer
			{
				uint32_t* indices = new uint32_t[s_Data.MaxIndices];
				uint32_t off = 0;
				for (uint32_t i = 0; i < s_Data.MaxIndices; i += 6)
				{ indices[i+0]=off; indices[i+1]=off+1; indices[i+2]=off+2; indices[i+3]=off+2; indices[i+4]=off+3; indices[i+5]=off; off+=4; }
				s_Data.VkQuadIB = dev->CreateBuffer(BufferDesc{
					s_Data.MaxIndices*sizeof(uint32_t), ResourceUsage::IndexBuffer, false, false, "Vk2D_QuadIB"
				});
				delete[] indices;
			}

			// Camera CB
			{
				BufferDesc d; d.Size=256; d.Usage=ResourceUsage::ConstantBuffer; d.CPUAccessible=true; d.DebugName="Vk2D_CameraCB";
				s_Data.VkCameraCB = dev->CreateBuffer(d);
			}

			// --- Shaders (use built-in triangle SPIR-V) ---
			auto& vsSpv = dev->GetTriangleVSSPIRV();
			auto& psSpv = dev->GetTrianglePSSPIRV();
			s_Data.VkQuadVS = dev->CreateShaderModule(vsSpv.data(), static_cast<uint32_t>(vsSpv.size()*4), "QuadVS");
			s_Data.VkQuadPS = dev->CreateShaderModule(psSpv.data(), static_cast<uint32_t>(psSpv.size()*4), "QuadPS");
			s_Data.VkCircleVS = s_Data.VkQuadVS; s_Data.VkCirclePS = s_Data.VkQuadPS;
			s_Data.VkLineVS   = s_Data.VkQuadVS; s_Data.VkLinePS   = s_Data.VkQuadPS;

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
				  s_Data.VkQuadPipeline=dev->CreateGraphicsPipeline(qd,s_Data.VkQuadVS,s_Data.VkQuadPS); }

				// Circle
				{ GraphicsPipelineDesc cd=pd; VertexInputLayout::VertexBinding b; b.Binding=0; b.Stride=sizeof(CircleVertex);
				  cd.VertexInput.Bindings.push_back(b);
				  cd.VertexInput.Attributes.push_back({0,0,RHIFormat::R32G32B32Float,0});
				  cd.VertexInput.Attributes.push_back({2,0,RHIFormat::R32G32B32A32Float,offsetof(CircleVertex,Color)});
				  s_Data.VkCirclePipeline=dev->CreateGraphicsPipeline(cd,s_Data.VkCircleVS,s_Data.VkCirclePS); }

				// Line
				{ GraphicsPipelineDesc ld=pd; ld.Topology=PrimitiveTopology::Lines; VertexInputLayout::VertexBinding b; b.Binding=0; b.Stride=sizeof(LineVertex);
				  ld.VertexInput.Bindings.push_back(b);
				  ld.VertexInput.Attributes.push_back({0,0,RHIFormat::R32G32B32Float,0});
				  ld.VertexInput.Attributes.push_back({1,0,RHIFormat::R32G32B32A32Float,offsetof(LineVertex,Color)});
				  s_Data.VkLinePipeline=dev->CreateGraphicsPipeline(ld,s_Data.VkLineVS,s_Data.VkLinePS); }
			}

			s_Data.WhiteTexture = Texture2D::Create(1, 1);
			uint32_t wtd=0xffffffff; s_Data.WhiteTexture->SetData(&wtd,sizeof(uint32_t));
			s_Data.TextureSlots[0]=s_Data.WhiteTexture;
			s_Data.QuadVertexPositions[0]={-0.5f,-0.5f,0.0f,1.0f}; s_Data.QuadVertexPositions[1]={0.5f,-0.5f,0.0f,1.0f};
			s_Data.QuadVertexPositions[2]={0.5f,0.5f,0.0f,1.0f}; s_Data.QuadVertexPositions[3]={-0.5f,0.5f,0.0f,1.0f};
			s_Data.CameraUniformBuffer=UniformBuffer::Create(sizeof(Renderer2DData::CameraData),0);

			CANDY_CORE_INFO("Renderer2D: Vulkan backend initialized (colored primitives only)");
			return;
		}

		if (s_Data.DX12Active)
		{
			CANDY_CORE_INFO("Renderer2D: initializing DX12 backend...");

			// Get DX12Device
			auto* gfxCtx = dynamic_cast<DX12GraphicsContext*>(
				Application::Get().GetWindow().GetGraphicsContext());
			if (!gfxCtx)
			{
				CANDY_CORE_ERROR("Renderer2D: DX12 API selected but no DX12GraphicsContext");
				s_Data.DX12Active = false;
				return;
			}
			s_Data.DX12Dev = gfxCtx->GetDevice();

			// --- CPU-side vertex buffers (same as OpenGL path, needed for batching) ---
			s_Data.QuadVertexBufferBase   = new QuadVertex[s_Data.MaxVertices];
			s_Data.CircleVertexBufferBase = new CircleVertex[s_Data.MaxVertices];
			s_Data.LineVertexBufferBase   = new LineVertex[s_Data.MaxVertices];

			// --- GPU vertex buffers (upload heap, CPU-writable) ---
			auto* dev = s_Data.DX12Dev;
			{
				BufferDesc vbDesc;
				vbDesc.Size          = s_Data.MaxVertices * sizeof(QuadVertex);
				vbDesc.Usage         = ResourceUsage::VertexBuffer | ResourceUsage::CopyDst;
				vbDesc.CPUAccessible = true;
				vbDesc.DebugName     = "Renderer2D_QuadVB";
				s_Data.DX12QuadVB = dev->CreateBuffer(vbDesc);
			}
			{
				BufferDesc vbDesc;
				vbDesc.Size          = s_Data.MaxVertices * sizeof(CircleVertex);
				vbDesc.Usage         = ResourceUsage::VertexBuffer | ResourceUsage::CopyDst;
				vbDesc.CPUAccessible = true;
				vbDesc.DebugName     = "Renderer2D_CircleVB";
				s_Data.DX12CircleVB = dev->CreateBuffer(vbDesc);
			}
			{
				BufferDesc vbDesc;
				vbDesc.Size          = s_Data.MaxVertices * sizeof(LineVertex);
				vbDesc.Usage         = ResourceUsage::VertexBuffer | ResourceUsage::CopyDst;
				vbDesc.CPUAccessible = true;
				vbDesc.DebugName     = "Renderer2D_LineVB";
				s_Data.DX12LineVB = dev->CreateBuffer(vbDesc);
			}

			// --- Index buffer (same pattern for quad + circle) ---
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
				s_Data.DX12QuadIB = dev->CreateGPUBufferWithData(
					quadIndices, s_Data.MaxIndices * sizeof(uint32_t),
					ResourceUsage::IndexBuffer, "Renderer2D_QuadIB");
				delete[] quadIndices;
			}

			// --- Camera constant buffer ---
			{
				BufferDesc cbDesc;
				cbDesc.Size          = 256; // 256-byte aligned
				cbDesc.Usage         = ResourceUsage::ConstantBuffer;
				cbDesc.CPUAccessible = true;
				cbDesc.DebugName     = "Renderer2D_CameraCB";
				s_Data.DX12CameraCB = dev->CreateBuffer(cbDesc);
			}

			// --- Compile HLSL shaders ---
			// Quad (textured)
			{
				static const char* quadVSSrc = R"(
cbuffer TransformCB : register(b0) { float4x4 u_ViewProjection; };
struct VSInput {
	float3 Position : POSITION; float4 Color : COLOR; float2 TexCoord : TEXCOORD;
	float  TexIndex : TEXINDEX; float TilingFactor : TILINGFACTOR; int EntityID : ENTITYID;
};
struct VSOutput { float4 Position : SV_POSITION; float4 Color : COLOR; float2 TexCoord : TEXCOORD; float TexIndex : TEXINDEX; float TilingFactor : TILINGFACTOR; int EntityID : ENTITYID; };
VSOutput VSMain(VSInput i) { VSOutput o; o.Position = mul(u_ViewProjection, float4(i.Position,1)); o.Color=i.Color; o.TexCoord=i.TexCoord; o.TexIndex=i.TexIndex; o.TilingFactor=i.TilingFactor; o.EntityID=i.EntityID; return o; }
)";
				auto blob = dev->CompileHLSL(quadVSSrc, "VSMain", "vs_5_0", "Renderer2D_QuadVS");
				if (blob)
					s_Data.DX12QuadVS = dev->CreateShaderModule(blob->GetBufferPointer(), static_cast<uint32_t>(blob->GetBufferSize()), "QuadVS");

				static const char* quadPSSrc = R"(
Texture2D u_Textures[32] : register(t0);
SamplerState u_Sampler : register(s0);
struct PSInput { float4 Position : SV_POSITION; float4 Color : COLOR; float2 TexCoord : TEXCOORD; float TexIndex : TEXINDEX; float TilingFactor : TILINGFACTOR; int EntityID : ENTITYID; };
struct PSOutput { float4 Color : SV_TARGET0; int EntityID : SV_TARGET1; };
PSOutput PSMain(PSInput i) { float2 uv = i.TexCoord * i.TilingFactor; int idx = max(0, min(31, int(i.TexIndex))); float4 texColor = i.Color * u_Textures[idx].Sample(u_Sampler, uv); PSOutput o; o.Color=texColor; o.EntityID=i.EntityID; return o; }
)";
				auto psBlob = dev->CompileHLSL(quadPSSrc, "PSMain", "ps_5_0", "Renderer2D_QuadPS");
				if (psBlob)
					s_Data.DX12QuadPS = dev->CreateShaderModule(psBlob->GetBufferPointer(), static_cast<uint32_t>(psBlob->GetBufferSize()), "QuadPS");
			}

			// Circle
			{
				static const char* circleVSSrc = R"(
cbuffer TransformCB : register(b0) { float4x4 u_ViewProjection; };
struct VSInput {
	float3 WorldPosition : POSITION; float3 LocalPosition : TEXCOORD0; float4 Color : COLOR;
	float Thickness : TEXCOORD1; float Fade : TEXCOORD2; int EntityID : ENTITYID;
};
struct VSOutput {
	float4 Position : SV_POSITION; float3 LocalPosition : LOCALPOS; float4 Color : COLOR;
	float Thickness : THICKNESS; float Fade : FADE; int EntityID : ENTITYID;
};
VSOutput VSMain(VSInput i) { VSOutput o; o.Position=mul(u_ViewProjection,float4(i.WorldPosition,1)); o.LocalPosition=i.LocalPosition; o.Color=i.Color; o.Thickness=i.Thickness; o.Fade=i.Fade; o.EntityID=i.EntityID; return o; }
)";
				auto blob = dev->CompileHLSL(circleVSSrc, "VSMain", "vs_5_0", "Renderer2D_CircleVS");
				if (blob)
					s_Data.DX12CircleVS = dev->CreateShaderModule(blob->GetBufferPointer(), static_cast<uint32_t>(blob->GetBufferSize()), "CircleVS");
			}
			{
				static const char* circlePSSrc = R"(
struct PSInput { float4 Position:SV_POSITION; float3 LocalPosition:LOCALPOS; float4 Color:COLOR; float Thickness:THICKNESS; float Fade:FADE; int EntityID:ENTITYID; };
struct PSOutput { float4 Color:SV_TARGET0; int EntityID:SV_TARGET1; };
PSOutput PSMain(PSInput i) { float d=1.0-length(i.LocalPosition); float c=smoothstep(0,i.Fade,d); c*=smoothstep(i.Thickness+i.Fade,i.Thickness,d); if(c==0)discard; PSOutput o; o.Color=i.Color; o.Color.a*=c; o.EntityID=i.EntityID; return o; }
)";
				auto blob = dev->CompileHLSL(circlePSSrc, "PSMain", "ps_5_0", "Renderer2D_CirclePS");
				if (blob)
					s_Data.DX12CirclePS = dev->CreateShaderModule(blob->GetBufferPointer(), static_cast<uint32_t>(blob->GetBufferSize()), "CirclePS");
			}

			// Line
			{
				static const char* lineVSSrc = R"(
cbuffer TransformCB : register(b0) { float4x4 u_ViewProjection; };
struct VSInput { float3 Position:POSITION; float4 Color:COLOR; int EntityID:ENTITYID; };
struct VSOutput { float4 Position:SV_POSITION; float4 Color:COLOR; int EntityID:ENTITYID; };
VSOutput VSMain(VSInput i) { VSOutput o; o.Position=mul(u_ViewProjection,float4(i.Position,1)); o.Color=i.Color; o.EntityID=i.EntityID; return o; }
)";
				auto blob = dev->CompileHLSL(lineVSSrc, "VSMain", "vs_5_0", "Renderer2D_LineVS");
				if (blob)
					s_Data.DX12LineVS = dev->CreateShaderModule(blob->GetBufferPointer(), static_cast<uint32_t>(blob->GetBufferSize()), "LineVS");
			}
			{
				static const char* linePSSrc = R"(
struct PSInput { float4 Position:SV_POSITION; float4 Color:COLOR; int EntityID:ENTITYID; };
struct PSOutput { float4 Color:SV_TARGET0; int EntityID:SV_TARGET1; };
PSOutput PSMain(PSInput i) { PSOutput o; o.Color=i.Color; o.EntityID=i.EntityID; return o; }
)";
				auto blob = dev->CompileHLSL(linePSSrc, "PSMain", "ps_5_0", "Renderer2D_LinePS");
				if (blob)
					s_Data.DX12LinePS = dev->CreateShaderModule(blob->GetBufferPointer(), static_cast<uint32_t>(blob->GetBufferSize()), "LinePS");
			}

			// --- Create textured root signature (shared by quad) ---
			s_Data.DX12TexturedRootSig = dev->CreateTexturedRootSignature();

			// --- Create pipelines ---
			{
				GraphicsPipelineDesc pipeDesc;
				pipeDesc.Topology           = PrimitiveTopology::Triangles;
				pipeDesc.Rasterizer.Cull    = CullMode::None;
				pipeDesc.Rasterizer.Fill    = FillMode::Solid;
				pipeDesc.DepthStencil.DepthTestEnable  = false;
				pipeDesc.DepthStencil.DepthWriteEnable = false;
				pipeDesc.Blend.BlendEnable         = true;
				pipeDesc.Blend.SrcColorBlendFactor = BlendState::BlendFactor::SrcAlpha;
				pipeDesc.Blend.DstColorBlendFactor = BlendState::BlendFactor::OneMinusSrcAlpha;
				pipeDesc.Blend.WriteMask           = ColorWriteMask::All;
				pipeDesc.RenderTargetFormats = { RHIFormat::R8G8B8A8Unorm, RHIFormat::R32Sint };

				// Quad pipeline (textured — uses TexturedRootSignature)
				{
					GraphicsPipelineDesc qd = pipeDesc;
					VertexInputLayout::VertexBinding binding;
					binding.Binding = 0;
					binding.Stride  = sizeof(QuadVertex);
					qd.VertexInput.Bindings.push_back(binding);

					qd.VertexInput.Attributes.push_back({ 0, 0, RHIFormat::R32G32B32Float,    0 });                       // Position
					qd.VertexInput.Attributes.push_back({ 1, 0, RHIFormat::R32G32B32A32Float, offsetof(QuadVertex, Color) });         // Color
					qd.VertexInput.Attributes.push_back({ 2, 0, RHIFormat::R32G32Float,       offsetof(QuadVertex, TexCoord) });      // TexCoord
					qd.VertexInput.Attributes.push_back({ 3, 0, RHIFormat::R32Float,          offsetof(QuadVertex, TexIndex) });      // TexIndex
					qd.VertexInput.Attributes.push_back({ 4, 0, RHIFormat::R32Float,          offsetof(QuadVertex, TilingFactor) });  // TilingFactor
					qd.VertexInput.Attributes.push_back({ 5, 0, RHIFormat::R32Sint,           offsetof(QuadVertex, EntityID) });      // EntityID

					if (s_Data.DX12TexturedRootSig)
					{
						// Create pipeline directly with textured root signature
						s_Data.DX12QuadPipeline = dev->CreateGraphicsPipelineWithRootSig(
							qd, s_Data.DX12QuadVS, s_Data.DX12QuadPS,
							s_Data.DX12TexturedRootSig.Get());
					}
					else
					{
						s_Data.DX12QuadPipeline = dev->CreateGraphicsPipeline(qd, s_Data.DX12QuadVS, s_Data.DX12QuadPS);
					}
				}

				// Circle pipeline
				{
					GraphicsPipelineDesc cd = pipeDesc;
					VertexInputLayout::VertexBinding binding;
					binding.Binding = 0;
					binding.Stride  = sizeof(CircleVertex);
					cd.VertexInput.Bindings.push_back(binding);

					cd.VertexInput.Attributes.push_back({ 0, 0, RHIFormat::R32G32B32Float,    0 });
					cd.VertexInput.Attributes.push_back({ 1, 0, RHIFormat::R32G32B32Float,    offsetof(CircleVertex, LocalPosition) });
					cd.VertexInput.Attributes.push_back({ 2, 0, RHIFormat::R32G32B32A32Float, offsetof(CircleVertex, Color) });
					cd.VertexInput.Attributes.push_back({ 3, 0, RHIFormat::R32Float,          offsetof(CircleVertex, Thickness) });
					cd.VertexInput.Attributes.push_back({ 4, 0, RHIFormat::R32Float,          offsetof(CircleVertex, Fade) });
					cd.VertexInput.Attributes.push_back({ 5, 0, RHIFormat::R32Sint,           offsetof(CircleVertex, EntityID) });

					s_Data.DX12CirclePipeline = dev->CreateGraphicsPipeline(cd, s_Data.DX12CircleVS, s_Data.DX12CirclePS);
				}

				// Line pipeline
				{
					GraphicsPipelineDesc ld = pipeDesc;
					ld.Topology = PrimitiveTopology::Lines;

					VertexInputLayout::VertexBinding binding;
					binding.Binding = 0;
					binding.Stride  = sizeof(LineVertex);
					ld.VertexInput.Bindings.push_back(binding);

					ld.VertexInput.Attributes.push_back({ 0, 0, RHIFormat::R32G32B32Float,    0 });
					ld.VertexInput.Attributes.push_back({ 1, 0, RHIFormat::R32G32B32A32Float, offsetof(LineVertex, Color) });
					ld.VertexInput.Attributes.push_back({ 2, 0, RHIFormat::R32Sint,           offsetof(LineVertex, EntityID) });

					s_Data.DX12LinePipeline = dev->CreateGraphicsPipeline(ld, s_Data.DX12LineVS, s_Data.DX12LinePS);
				}
			}

			// --- White texture (1x1 white pixel) ---
			s_Data.WhiteTexture = Texture2D::Create(1, 1);
			uint32_t whiteTextureData = 0xffffffff;
			s_Data.WhiteTexture->SetData(&whiteTextureData, sizeof(uint32_t));
			s_Data.TextureSlots[0] = s_Data.WhiteTexture;

			s_Data.QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
			s_Data.QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
			s_Data.QuadVertexPositions[2] = {  0.5f,  0.5f, 0.0f, 1.0f };
			s_Data.QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

			// Set up dummy uniform buffer (not used for DX12, camera set via CB)
			s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(Renderer2DData::CameraData), 0);

			CANDY_CORE_INFO("Renderer2D: DX12 backend initialized");
			return;
		}

		// =====================================================
		// OpenGL path (unchanged)
		// =====================================================

		s_Data.QuadVertexArray = VertexArray::Create();

		s_Data.QuadVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(QuadVertex));
		s_Data.QuadVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float4, "a_Color" },
			{ ShaderDataType::Float2, "a_TexCoord" },
			{ ShaderDataType::Float,  "a_TexIndex" },
			{ ShaderDataType::Float,  "a_TilingFactor" },
			{ ShaderDataType::Int,    "a_EntityID"}
			});
		s_Data.QuadVertexArray->AddVertexBuffer(s_Data.QuadVertexBuffer);

		s_Data.QuadVertexBufferBase = new QuadVertex[s_Data.MaxVertices];

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

		Ref<IndexBuffer> quadIB = IndexBuffer::Create(quadIndices, s_Data.MaxIndices);
		s_Data.QuadVertexArray->SetIndexBuffer(quadIB);
		delete[] quadIndices;

		s_Data.CircleVertexArray = VertexArray::Create();
		s_Data.CircleVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(CircleVertex));
		s_Data.CircleVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_WorldPosition" },
			{ ShaderDataType::Float3, "a_LocalPosition" },
			{ ShaderDataType::Float4, "a_Color"         },
			{ ShaderDataType::Float,  "a_Thickness"     },
			{ ShaderDataType::Float,  "a_Fade"          },
			{ ShaderDataType::Int,    "a_EntityID"      }
			});
		s_Data.CircleVertexArray->AddVertexBuffer(s_Data.CircleVertexBuffer);
		s_Data.CircleVertexArray->SetIndexBuffer(quadIB); // Use quad IB
		s_Data.CircleVertexBufferBase = new CircleVertex[s_Data.MaxVertices];

		// Lines
		s_Data.LineVertexArray = VertexArray::Create();

		s_Data.LineVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(LineVertex));
		s_Data.LineVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float4, "a_Color"    },
			{ ShaderDataType::Int,    "a_EntityID" }
			});
		s_Data.LineVertexArray->AddVertexBuffer(s_Data.LineVertexBuffer);
		s_Data.LineVertexBufferBase = new LineVertex[s_Data.MaxVertices];


		s_Data.WhiteTexture = Texture2D::Create(1, 1);
		uint32_t whiteTextureData = 0xffffffff;
		s_Data.WhiteTexture->SetData(&whiteTextureData, sizeof(uint32_t));

		int32_t samplers[s_Data.MaxTextureSlots];
		for (uint32_t i = 0; i < s_Data.MaxTextureSlots; i++)
			samplers[i] = i;

		auto loadShader = [](const char* name, const char* vfsPath) -> Ref<Shader> {
			auto source = FileSystem::Get().ReadText(vfsPath);
			if (source)
				return Shader::CreateFromSource(name, *source);
			CANDY_CORE_ERROR("Failed to load shader: {0}", vfsPath);
			return nullptr;
		};
		s_Data.QuadShader = loadShader("Renderer2D_Quad", "VFS://Engine/Shaders/Renderer2D_Quad.glsl");
		s_Data.CircleShader = loadShader("Renderer2D_Circle", "VFS://Engine/Shaders/Renderer2D_Circle.glsl");
		s_Data.LineShader = loadShader("Renderer2D_Line", "VFS://Engine/Shaders/Renderer2D_Line.glsl");

		s_Data.TextureSlots[0] = s_Data.WhiteTexture;

		s_Data.QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[1] = { 0.5f, -0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[2] = { 0.5f,  0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };
		s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(Renderer2DData::CameraData), 0);
	}

	void Renderer2D::Shutdown()
	{
		delete[] s_Data.QuadVertexBufferBase;
		if (s_Data.DX12Active)
		{
			delete[] s_Data.CircleVertexBufferBase;
			delete[] s_Data.LineVertexBufferBase;
			s_Data.DX12QuadVB.reset();
			s_Data.DX12QuadIB.reset();
			s_Data.DX12CircleVB.reset();
			s_Data.DX12LineVB.reset();
			s_Data.DX12CameraCB.reset();
			s_Data.DX12QuadPipeline.reset();
			s_Data.DX12CirclePipeline.reset();
			s_Data.DX12LinePipeline.reset();
			s_Data.DX12QuadVS.reset(); s_Data.DX12QuadPS.reset();
			s_Data.DX12CircleVS.reset(); s_Data.DX12CirclePS.reset();
			s_Data.DX12LineVS.reset(); s_Data.DX12LinePS.reset();
			s_Data.DX12TexturedRootSig.Reset();
		}
		if (s_Data.VkActive)
		{
			delete[] s_Data.CircleVertexBufferBase;
			delete[] s_Data.LineVertexBufferBase;
			s_Data.VkQuadVB.reset(); s_Data.VkQuadIB.reset(); s_Data.VkCircleVB.reset();
			s_Data.VkLineVB.reset(); s_Data.VkCameraCB.reset();
			s_Data.VkQuadPipeline.reset(); s_Data.VkCirclePipeline.reset(); s_Data.VkLinePipeline.reset();
			if (s_Data.VkDev)
			{
				VkDevice vd = s_Data.VkDev->GetVkDevice();
				if (s_Data.VkDescSet)  s_Data.VkDev->fnFreeCommandBuffers(vd, VK_NULL_HANDLE, 0, nullptr); // pool auto-frees sets
				if (s_Data.VkDescPool) s_Data.VkDev->fnDestroyDescriptorPool(vd, s_Data.VkDescPool, nullptr);
				if (s_Data.VkDescLayout) s_Data.VkDev->fnDestroyDescriptorSetLayout(vd, s_Data.VkDescLayout, nullptr);
			}
		}
	}

	void Renderer2D::BeginScene(const OrthographicCamera& camera)
	{
		s_Data.QuadShader->Bind();
		s_Data.QuadShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());

		StartBatch();
	}

	void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		s_Data.CameraBuffer.ViewProjection = camera.GetProjection() * glm::inverse(transform);
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer2DData::CameraData));

		StartBatch();
	}

	void Renderer2D::BeginScene(const EditorCamera& camera)
	{
		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjection();
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer2DData::CameraData));

		StartBatch();
	}

	void Renderer2D::EndScene()
	{
		if (!s_Data.DX12Active)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.QuadVertexBufferPtr - (uint8_t*)s_Data.QuadVertexBufferBase);
			s_Data.QuadVertexBuffer->SetData(s_Data.QuadVertexBufferBase, dataSize);
		}

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
		if (s_Data.VkActive)
		{
			auto* dev = s_Data.VkDev;
			if (!dev) return;

			// Upload camera CB
			{
				auto* vkCB = dynamic_cast<VulkanBuffer*>(s_Data.VkCameraCB.get());
				if (vkCB) { void* m = vkCB->Map(); memcpy(m, &s_Data.CameraBuffer, sizeof(s_Data.CameraBuffer)); vkCB->Unmap(); }
			}

			// Update descriptor set
			{
				auto* vkCB = dynamic_cast<VulkanBuffer*>(s_Data.VkCameraCB.get());
				if (vkCB)
				{
					VkDescriptorBufferInfo bufInfo = {};
					bufInfo.buffer = vkCB->GetVkBuffer();
					bufInfo.offset = 0;
					bufInfo.range  = VK_WHOLE_SIZE;

					VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					write.dstSet          = s_Data.VkDescSet;
					write.dstBinding      = 0;
					write.descriptorCount = 1;
					write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					write.pBufferInfo     = &bufInfo;
					dev->fnUpdateDescriptorSets(dev->GetVkDevice(), 1, &write, 0, nullptr);
				}
			}

			auto& queue = dev->GetCommandQueue();
			auto  cmd   = queue.CreateCommandBuffer();
			if (!cmd) return;

			cmd->Begin();

			RenderPassDesc rpDesc;
			rpDesc.ColorAttachments.push_back({RHIFormat::R8G8B8A8Unorm, LoadOp::Clear, {0.1f,0.1f,0.15f,1.0f}});
			cmd->BeginRenderPass(rpDesc);
			cmd->SetViewport(0,0,1280.f,720.f); cmd->SetScissor(0,0,1280,720);

			VkDevice vd = dev->GetVkDevice();

			auto uploadVkVB = [&](Ref<RHIBuffer>& vb, const void* data, uint32_t size) -> bool {
				auto* vkBuf = dynamic_cast<VulkanBuffer*>(vb.get());
				if (vkBuf) { void* m = vkBuf->Map(); memcpy(m, data, size); vkBuf->Unmap(); }
				return vkBuf != nullptr;
			};

			// Quad batch
			if (s_Data.QuadIndexCount)
			{
				uint32_t sz = static_cast<uint32_t>((uint8_t*)s_Data.QuadVertexBufferPtr - (uint8_t*)s_Data.QuadVertexBufferBase);
				uploadVkVB(s_Data.VkQuadVB, s_Data.QuadVertexBufferBase, sz);
				cmd->SetPipeline(s_Data.VkQuadPipeline);
				cmd->SetConstantBuffer(0, 0, s_Data.VkCameraCB);
				cmd->SetVertexBuffer(s_Data.VkQuadVB);
				cmd->SetIndexBuffer(s_Data.VkQuadIB);
				cmd->DrawIndexed(s_Data.QuadIndexCount);
				s_Data.Stats.DrawCalls++;
			}

			// Circle batch
			if (s_Data.CircleIndexCount)
			{
				uint32_t sz = static_cast<uint32_t>((uint8_t*)s_Data.CircleVertexBufferPtr - (uint8_t*)s_Data.CircleVertexBufferBase);
				uploadVkVB(s_Data.VkCircleVB, s_Data.CircleVertexBufferBase, sz);
				cmd->SetPipeline(s_Data.VkCirclePipeline);
				cmd->SetConstantBuffer(0, 0, s_Data.VkCameraCB);
				cmd->SetVertexBuffer(s_Data.VkCircleVB);
				cmd->SetIndexBuffer(s_Data.VkQuadIB);
				cmd->DrawIndexed(s_Data.CircleIndexCount);
				s_Data.Stats.DrawCalls++;
			}

			// Line batch
			if (s_Data.LineVertexCount)
			{
				uint32_t sz = static_cast<uint32_t>((uint8_t*)s_Data.LineVertexBufferPtr - (uint8_t*)s_Data.LineVertexBufferBase);
				uploadVkVB(s_Data.VkLineVB, s_Data.LineVertexBufferBase, sz);
				cmd->SetPipeline(s_Data.VkLinePipeline);
				cmd->SetConstantBuffer(0, 0, s_Data.VkCameraCB);
				cmd->SetVertexBuffer(s_Data.VkLineVB);
				cmd->Draw(s_Data.LineVertexCount);
				s_Data.Stats.DrawCalls++;
			}

			cmd->EndRenderPass();
			cmd->End();
			queue.Submit({cmd.get()});
			dev->WaitIdle();
			return;
		}

		if (s_Data.DX12Active)
		{
			// =====================================================
			// DX12 path — upload vertex data + record draws
			// =====================================================
			auto* dev = s_Data.DX12Dev;
			if (!dev) return;

			// Upload camera constant buffer
			{
				auto* dx12CB = dynamic_cast<DX12Buffer*>(s_Data.DX12CameraCB.get());
				if (dx12CB && dx12CB->GetResource())
				{
					void* mapped = dx12CB->Map();
					memcpy(mapped, &s_Data.CameraBuffer, sizeof(s_Data.CameraBuffer));
					dx12CB->Unmap();
				}
			}

			auto& queue = dev->GetCommandQueue();
			auto  cmd   = queue.CreateCommandBuffer();
			if (!cmd) return;
			auto* dx12cb = static_cast<DX12CommandBuffer*>(cmd.get());

			// Set render target
			if (s_Data.DX12ActiveFramebuffer)
			{
				dx12cb->SetFramebufferRenderTarget(s_Data.DX12ActiveFramebuffer);
			}
			else
			{
				// Fallback to swap chain (for non-viewport rendering)
				auto* gfxCtx = dynamic_cast<DX12GraphicsContext*>(
					Application::Get().GetWindow().GetGraphicsContext());
				if (gfxCtx)
					dx12cb->SetSwapChainRenderTarget(gfxCtx->GetSwapChain());
			}

			cmd->Begin();

			RenderPassDesc rpDesc;
			{
				RenderPassColorAttachment colorAttachment;
				colorAttachment.Format = RHIFormat::R8G8B8A8Unorm;
				colorAttachment.LoadOp = LoadOp::Clear;
				colorAttachment.ClearColor[0] = 0.1f;
				colorAttachment.ClearColor[1] = 0.1f;
				colorAttachment.ClearColor[2] = 0.1f;
				colorAttachment.ClearColor[3] = 1.0f;
				rpDesc.ColorAttachments.push_back(colorAttachment);

				// Entity ID attachment
				if (s_Data.DX12ActiveFramebuffer && s_Data.DX12ActiveFramebuffer->GetColorAttachmentCount() > 1)
				{
					RenderPassColorAttachment idAttachment;
					idAttachment.Format = RHIFormat::R32Sint;
					idAttachment.LoadOp = LoadOp::Clear;
					idAttachment.ClearColor[0] = -1.0f;
					idAttachment.ClearColor[1] = -1.0f;
					idAttachment.ClearColor[2] = -1.0f;
					idAttachment.ClearColor[3] = -1.0f;
					rpDesc.ColorAttachments.push_back(idAttachment);
				}
			}
			cmd->BeginRenderPass(rpDesc);

			// Viewport + scissor
			uint32_t vpW = 1280, vpH = 720;
			if (s_Data.DX12ActiveFramebuffer)
			{
				vpW = s_Data.DX12ActiveFramebuffer->GetSpecification().Width;
				vpH = s_Data.DX12ActiveFramebuffer->GetSpecification().Height;
			}
			cmd->SetViewport(0, 0, static_cast<float>(vpW), static_cast<float>(vpH));
			cmd->SetScissor(0, 0, vpW, vpH);

			// --- Quad batch (textured) ---
			if (s_Data.QuadIndexCount && s_Data.DX12QuadPipeline)
			{
				uint32_t dataSize = static_cast<uint32_t>(
					reinterpret_cast<uint8_t*>(s_Data.QuadVertexBufferPtr) -
					reinterpret_cast<uint8_t*>(s_Data.QuadVertexBufferBase));

				auto* dx12VB = dynamic_cast<DX12Buffer*>(s_Data.DX12QuadVB.get());
				if (dx12VB)
				{
					void* mapped = dx12VB->Map();
					memcpy(mapped, s_Data.QuadVertexBufferBase, dataSize);
					dx12VB->Unmap();
				}

				cmd->SetPipeline(s_Data.DX12QuadPipeline);
				cmd->SetConstantBuffer(0, 0, s_Data.DX12CameraCB);
				cmd->SetVertexBuffer(s_Data.DX12QuadVB);
				cmd->SetIndexBuffer(s_Data.DX12QuadIB);

				// --- Bind textures via descriptor table ---
				ID3D12DescriptorHeap* srvHeap = dev->GetCBVSRVUAVHeap();
				uint32_t descSize = dev->GetCBVSRVDescriptorSize();
				if (srvHeap && s_Data.DX12TexturedRootSig)
				{
					// Allocate 32 SRV slots at descriptor range start (slot 0)
					constexpr uint32_t kBaseSlot = 0;
					D3D12_CPU_DESCRIPTOR_HANDLE cpuBase = srvHeap->GetCPUDescriptorHandleForHeapStart();
					cpuBase.ptr += static_cast<SIZE_T>(kBaseSlot) * descSize;

					// Write SRV descriptors for each texture slot
					for (uint32_t i = 0; i < Renderer2DData::MaxTextureSlots; ++i)
					{
						D3D12_CPU_DESCRIPTOR_HANDLE dstCPU = cpuBase;
						dstCPU.ptr += static_cast<SIZE_T>(i) * descSize;

						auto* dx12Tex = dynamic_cast<DX12Texture2D*>(s_Data.TextureSlots[i].get());
						if (dx12Tex && dx12Tex->GetRHI() && dx12Tex->GetRHI()->GetResource())
						{
							// Copy SRV descriptor from the texture's slot
							D3D12_CPU_DESCRIPTOR_HANDLE srcCPU = srvHeap->GetCPUDescriptorHandleForHeapStart();
							srcCPU.ptr += static_cast<SIZE_T>(dx12Tex->GetRHI() ? 160 : 0) * descSize;

							// Rather than copy, just re-create the SRV at the target slot
							dx12Tex->GetRHI()->CreateSRV(srvHeap, kBaseSlot + i, descSize);
						}
						else
						{
							// White texture fallback: use the first Texture2D (slot 0)
							auto* whiteTex = dynamic_cast<DX12Texture2D*>(s_Data.TextureSlots[0].get());
							if (whiteTex && whiteTex->GetRHI())
								whiteTex->GetRHI()->CreateSRV(srvHeap, kBaseSlot + i, descSize);
						}
					}

					// Bind descriptor table (root parameter 1)
					D3D12_GPU_DESCRIPTOR_HANDLE gpuTable = srvHeap->GetGPUDescriptorHandleForHeapStart();
					gpuTable.ptr += static_cast<SIZE_T>(kBaseSlot) * descSize;
					dx12cb->GetNativeCommandList()->SetGraphicsRootDescriptorTable(1, gpuTable);
				}

				cmd->DrawIndexed(s_Data.QuadIndexCount);
				s_Data.Stats.DrawCalls++;
			}

			// --- Circle batch ---
			if (s_Data.CircleIndexCount && s_Data.DX12CirclePipeline)
			{
				uint32_t dataSize = static_cast<uint32_t>(
					reinterpret_cast<uint8_t*>(s_Data.CircleVertexBufferPtr) -
					reinterpret_cast<uint8_t*>(s_Data.CircleVertexBufferBase));

				auto* dx12VB = dynamic_cast<DX12Buffer*>(s_Data.DX12CircleVB.get());
				if (dx12VB)
				{
					void* mapped = dx12VB->Map();
					memcpy(mapped, s_Data.CircleVertexBufferBase, dataSize);
					dx12VB->Unmap();
				}

				cmd->SetPipeline(s_Data.DX12CirclePipeline);
				cmd->SetConstantBuffer(0, 0, s_Data.DX12CameraCB);
				cmd->SetVertexBuffer(s_Data.DX12CircleVB);
				cmd->SetIndexBuffer(s_Data.DX12QuadIB); // reuse quad IB
				cmd->DrawIndexed(s_Data.CircleIndexCount);
				s_Data.Stats.DrawCalls++;
			}

			// --- Line batch ---
			if (s_Data.LineVertexCount && s_Data.DX12LinePipeline)
			{
				uint32_t dataSize = static_cast<uint32_t>(
					reinterpret_cast<uint8_t*>(s_Data.LineVertexBufferPtr) -
					reinterpret_cast<uint8_t*>(s_Data.LineVertexBufferBase));

				auto* dx12VB = dynamic_cast<DX12Buffer*>(s_Data.DX12LineVB.get());
				if (dx12VB)
				{
					void* mapped = dx12VB->Map();
					memcpy(mapped, s_Data.LineVertexBufferBase, dataSize);
					dx12VB->Unmap();
				}

				cmd->SetPipeline(s_Data.DX12LinePipeline);
				cmd->SetConstantBuffer(0, 0, s_Data.DX12CameraCB);
				cmd->SetVertexBuffer(s_Data.DX12LineVB);
				cmd->Draw(s_Data.LineVertexCount);
				s_Data.Stats.DrawCalls++;
			}

			cmd->EndRenderPass();
			cmd->End();

			queue.Submit({ cmd.get() });
			// No Present when targeting a framebuffer
			dev->WaitIdle();
			return;
		}

		// =====================================================
		// OpenGL path (unchanged)
		// =====================================================
		if (s_Data.QuadIndexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.QuadVertexBufferPtr - (uint8_t*)s_Data.QuadVertexBufferBase);
			s_Data.QuadVertexBuffer->SetData(s_Data.QuadVertexBufferBase, dataSize);

			// Bind textures
			for (uint32_t i = 0; i < s_Data.TextureSlotIndex; i++)
				s_Data.TextureSlots[i]->Bind(i);

			s_Data.QuadShader->Bind();
			RenderCommand::DrawIndexed(s_Data.QuadVertexArray, PrimitiveTopology::Triangles, s_Data.QuadIndexCount);
			s_Data.Stats.DrawCalls++;
		}

		if (s_Data.CircleIndexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.CircleVertexBufferPtr - (uint8_t*)s_Data.CircleVertexBufferBase);
			s_Data.CircleVertexBuffer->SetData(s_Data.CircleVertexBufferBase, dataSize);

			s_Data.CircleShader->Bind();
			RenderCommand::DrawIndexed(s_Data.CircleVertexArray, PrimitiveTopology::Triangles, s_Data.CircleIndexCount);
			s_Data.Stats.DrawCalls++;
		}

		if (s_Data.LineVertexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.LineVertexBufferPtr - (uint8_t*)s_Data.LineVertexBufferBase);
			s_Data.LineVertexBuffer->SetData(s_Data.LineVertexBufferBase, dataSize);

			s_Data.LineShader->Bind();
			//RenderCommand::SetLineWidth(s_Data.LineWidth);
			RenderCommand::Draw(s_Data.LineVertexArray, PrimitiveTopology::Lines, s_Data.LineVertexCount);
			s_Data.Stats.DrawCalls++;
		}
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

	void Renderer2D::SetDX12ActiveFramebuffer(DX12Framebuffer* fb)
	{
		s_Data.DX12ActiveFramebuffer = fb;
	}
}

