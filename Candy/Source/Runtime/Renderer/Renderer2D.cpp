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
#include "Runtime/RHI/RHICommandQueue.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// D3D12 backend includes
#include "Platform/D3D12/D3D12Device.h"
#include "Platform/D3D12/D3D12GraphicsContext.h"
#include "Platform/D3D12/D3D12Buffer.h"
#include "Platform/D3D12/D3D12PipelineState.h"
#include "Platform/D3D12/D3D12Framebuffer.h"
#include "Platform/D3D12/D3D12CommandBuffer.h"
#include "Platform/D3D12/D3D12Texture2D.h"
#include "Platform/D3D12/D3D12Texture.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanGraphicsContext.h"
#include "Platform/Vulkan/VulkanBuffer.h"
#include "Platform/Vulkan/VulkanPipelineState.h"

// OpenGL RHI backend includes
#include "Platform/OpenGL/OpenGLRHIDevice.h"
#include "Platform/OpenGL/OpenGLRHICommandBuffer.h"
#include "Platform/OpenGL/OpenGLRHIResources.h"
#include "Runtime/RHI/RHIContext.h"


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

		// ---- D3D12 backend data ----
		bool D3D12Active = false;
		D3D12Device* D3D12Dev = nullptr;

		// Vertex buffers (upload heap)
		Ref<RHIBuffer> D3D12QuadVB;
		Ref<RHIBuffer> D3D12QuadIB;
		Ref<RHIBuffer> D3D12CircleVB;
		Ref<RHIBuffer> D3D12LineVB;
		Ref<RHIBuffer> D3D12CameraCB; // constant buffer for ViewProjection

		// Pipelines
		Ref<RHIGraphicsPipeline> D3D12QuadPipeline;
		Ref<RHIGraphicsPipeline> D3D12CirclePipeline;
		Ref<RHIGraphicsPipeline> D3D12LinePipeline;

		// Shader modules
		Ref<RHIShaderModule> D3D12QuadVS, D3D12QuadPS;
		Ref<RHIShaderModule> D3D12CircleVS, D3D12CirclePS;
		Ref<RHIShaderModule> D3D12LineVS, D3D12LinePS;

		// Textured root signature (shared by quad pipeline)
		Microsoft::WRL::ComPtr<ID3D12RootSignature> D3D12TexturedRootSig;

		// Active render target for D3D12/OpenGL/Vulkan flushing (RHI-bridged).
		Ref<RHIFramebuffer> ActiveRenderTarget;

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

		// ---- OpenGL RHI backend data ----
		bool OL_Active = false;
		Ref<RHIBuffer>           OL_QuadVB;
		Ref<RHIBuffer>           OL_QuadIB;
		Ref<RHIBuffer>           OL_CircleVB;
		Ref<RHIBuffer>           OL_LineVB;
		Ref<RHIBuffer>           OL_CameraCB;
		Ref<RHIGraphicsPipeline> OL_QuadPipeline;
		Ref<RHIGraphicsPipeline> OL_CirclePipeline;
		Ref<RHIGraphicsPipeline> OL_LinePipeline;
		Ref<RHIShaderModule>     OL_QuadShader;
		Ref<RHIShaderModule>     OL_CircleShader;
		Ref<RHIShaderModule>     OL_LineShader;
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
			s_Data.VkQuadVB   = makeUploadVB(s_Data.MaxVertices * sizeof(QuadVertex),  sizeof(QuadVertex),  "Vk2D_QuadVB");
			s_Data.VkCircleVB = makeUploadVB(s_Data.MaxVertices * sizeof(CircleVertex),sizeof(CircleVertex),"Vk2D_CircleVB");
			s_Data.VkLineVB   = makeUploadVB(s_Data.MaxVertices * sizeof(LineVertex),  sizeof(LineVertex),  "Vk2D_LineVB");

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
					s_Data.VkQuadIB = dev->CreateBuffer(ibDesc);
				}
				// Map + upload
				auto* vkIB = dynamic_cast<VulkanBuffer*>(s_Data.VkQuadIB.get());
				if (vkIB)
				{
					void* m = vkIB->Map();
					memcpy(m, indices, s_Data.MaxIndices * sizeof(uint32_t));
					vkIB->Unmap();
				}
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

		if (s_Data.D3D12Active)
		{
			CANDY_CORE_INFO("Renderer2D: initializing D3D12 backend...");

			// Get D3D12Device
			auto* gfxCtx = dynamic_cast<D3D12GraphicsContext*>(
				Application::Get().GetWindow().GetGraphicsContext());
			if (!gfxCtx)
			{
				CANDY_CORE_ERROR("Renderer2D: D3D12 API selected but no D3D12GraphicsContext");
				s_Data.D3D12Active = false;
				return;
			}
			s_Data.D3D12Dev = gfxCtx->GetDevice();

			// --- CPU-side vertex buffers (same as OpenGL path, needed for batching) ---
			s_Data.QuadVertexBufferBase   = new QuadVertex[s_Data.MaxVertices];
			s_Data.CircleVertexBufferBase = new CircleVertex[s_Data.MaxVertices];
			s_Data.LineVertexBufferBase   = new LineVertex[s_Data.MaxVertices];

		// --- GPU vertex buffers (upload heap, CPU-writable) ---
		auto* dev = s_Data.D3D12Dev;
		{
			BufferDesc vbDesc;
			vbDesc.Size          = s_Data.MaxVertices * sizeof(QuadVertex);
			vbDesc.Usage         = ResourceUsage::VertexBuffer | ResourceUsage::CopyDst;
			vbDesc.CPUAccessible = true;
			vbDesc.Stride        = sizeof(QuadVertex);
			vbDesc.DebugName     = "Renderer2D_QuadVB";
			s_Data.D3D12QuadVB = dev->CreateBuffer(vbDesc);
		}
		{
			BufferDesc vbDesc;
			vbDesc.Size          = s_Data.MaxVertices * sizeof(CircleVertex);
			vbDesc.Usage         = ResourceUsage::VertexBuffer | ResourceUsage::CopyDst;
			vbDesc.CPUAccessible = true;
			vbDesc.Stride        = sizeof(CircleVertex);
			vbDesc.DebugName     = "Renderer2D_CircleVB";
			s_Data.D3D12CircleVB = dev->CreateBuffer(vbDesc);
		}
		{
			BufferDesc vbDesc;
			vbDesc.Size          = s_Data.MaxVertices * sizeof(LineVertex);
			vbDesc.Usage         = ResourceUsage::VertexBuffer | ResourceUsage::CopyDst;
			vbDesc.CPUAccessible = true;
			vbDesc.Stride        = sizeof(LineVertex);
			vbDesc.DebugName     = "Renderer2D_LineVB";
			s_Data.D3D12LineVB = dev->CreateBuffer(vbDesc);
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
				s_Data.D3D12QuadIB = dev->CreateGPUBufferWithData(
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
				s_Data.D3D12CameraCB = dev->CreateBuffer(cbDesc);
			}

			// --- Compile HLSL shaders ---
			// Quad (textured)
			{
				static const char* quadVSSrc = R"(
cbuffer TransformCB : register(b0) { float4x4 u_ViewProjection; };
struct VSInput {
	float3 Position : TEXCOORD0; float4 Color : TEXCOORD1; float2 TexCoord : TEXCOORD2;
	float  TexIndex : TEXCOORD3; float TilingFactor : TEXCOORD4; int EntityID : TEXCOORD5;
};
struct VSOutput { float4 Position : SV_POSITION; float4 Color : COLOR; float2 TexCoord : TEXCOORD; float TexIndex : TEXINDEX; float TilingFactor : TILINGFACTOR; int EntityID : ENTITYID; };
VSOutput VSMain(VSInput i) { VSOutput o; o.Position = mul(u_ViewProjection, float4(i.Position,1)); o.Color=i.Color; o.TexCoord=i.TexCoord; o.TexIndex=i.TexIndex; o.TilingFactor=i.TilingFactor; o.EntityID=i.EntityID; return o; }
)";
				auto blob = dev->CompileHLSL(quadVSSrc, "VSMain", "vs_5_0", "Renderer2D_QuadVS");
				if (blob)
					s_Data.D3D12QuadVS = dev->CreateShaderModule(blob->GetBufferPointer(), static_cast<uint32_t>(blob->GetBufferSize()), "QuadVS");

				static const char* quadPSSrc = R"(
Texture2D u_Textures[32] : register(t0);
SamplerState u_Sampler : register(s0);
struct PSInput { float4 Position : SV_POSITION; float4 Color : COLOR; float2 TexCoord : TEXCOORD; float TexIndex : TEXINDEX; float TilingFactor : TILINGFACTOR; int EntityID : ENTITYID; };
struct PSOutput { float4 Color : SV_TARGET0; int EntityID : SV_TARGET1; };
PSOutput PSMain(PSInput i)
{
	float2 uv = i.TexCoord * i.TilingFactor;
	int idx = max(0, min(31, int(i.TexIndex)));
	float4 texColor = i.Color;
	// SM5.0 forbids dynamic indexing into Texture2D[32]; unroll into 32
	// literal-indexed Sample calls so the bindless-ish pattern is valid.
	[unroll] for (int t = 0; t < 32; ++t)
	{
		[unroll] if (t == idx)
			texColor = i.Color * u_Textures[t].Sample(u_Sampler, uv);
	}
	PSOutput o;
	o.Color = texColor;
	o.EntityID = i.EntityID;
	return o;
}
)";
				auto psBlob = dev->CompileHLSL(quadPSSrc, "PSMain", "ps_5_0", "Renderer2D_QuadPS");
				if (psBlob)
					s_Data.D3D12QuadPS = dev->CreateShaderModule(psBlob->GetBufferPointer(), static_cast<uint32_t>(psBlob->GetBufferSize()), "QuadPS");
			}

			// Circle
			{
				static const char* circleVSSrc = R"(
cbuffer TransformCB : register(b0) { float4x4 u_ViewProjection; };
struct VSInput {
	float3 WorldPosition : TEXCOORD0; float3 LocalPosition : TEXCOORD1; float4 Color : TEXCOORD2;
	float Thickness : TEXCOORD3; float Fade : TEXCOORD4; int EntityID : TEXCOORD5;
};
struct VSOutput {
	float4 Position : SV_POSITION; float3 LocalPosition : LOCALPOS; float4 Color : COLOR;
	float Thickness : THICKNESS; float Fade : FADE; int EntityID : ENTITYID;
};
VSOutput VSMain(VSInput i) { VSOutput o; o.Position=mul(u_ViewProjection,float4(i.WorldPosition,1)); o.LocalPosition=i.LocalPosition; o.Color=i.Color; o.Thickness=i.Thickness; o.Fade=i.Fade; o.EntityID=i.EntityID; return o; }
)";
				auto blob = dev->CompileHLSL(circleVSSrc, "VSMain", "vs_5_0", "Renderer2D_CircleVS");
				if (blob)
					s_Data.D3D12CircleVS = dev->CreateShaderModule(blob->GetBufferPointer(), static_cast<uint32_t>(blob->GetBufferSize()), "CircleVS");
			}
			{
				static const char* circlePSSrc = R"(
struct PSInput { float4 Position:SV_POSITION; float3 LocalPosition:LOCALPOS; float4 Color:COLOR; float Thickness:THICKNESS; float Fade:FADE; int EntityID:ENTITYID; };
struct PSOutput { float4 Color:SV_TARGET0; int EntityID:SV_TARGET1; };
PSOutput PSMain(PSInput i) { float d=1.0-length(i.LocalPosition); float c=smoothstep(0,i.Fade,d); c*=smoothstep(i.Thickness+i.Fade,i.Thickness,d); if(c==0)discard; PSOutput o; o.Color=i.Color; o.Color.a*=c; o.EntityID=i.EntityID; return o; }
)";
				auto blob = dev->CompileHLSL(circlePSSrc, "PSMain", "ps_5_0", "Renderer2D_CirclePS");
				if (blob)
					s_Data.D3D12CirclePS = dev->CreateShaderModule(blob->GetBufferPointer(), static_cast<uint32_t>(blob->GetBufferSize()), "CirclePS");
			}

			// Line
			{
				static const char* lineVSSrc = R"(
cbuffer TransformCB : register(b0) { float4x4 u_ViewProjection; };
struct VSInput { float3 Position:TEXCOORD0; float4 Color:TEXCOORD1; int EntityID:TEXCOORD2; };
struct VSOutput { float4 Position:SV_POSITION; float4 Color:COLOR; int EntityID:ENTITYID; };
VSOutput VSMain(VSInput i) { VSOutput o; o.Position=mul(u_ViewProjection,float4(i.Position,1)); o.Color=i.Color; o.EntityID=i.EntityID; return o; }
)";
				auto blob = dev->CompileHLSL(lineVSSrc, "VSMain", "vs_5_0", "Renderer2D_LineVS");
				if (blob)
					s_Data.D3D12LineVS = dev->CreateShaderModule(blob->GetBufferPointer(), static_cast<uint32_t>(blob->GetBufferSize()), "LineVS");
			}
			{
				static const char* linePSSrc = R"(
struct PSInput { float4 Position:SV_POSITION; float4 Color:COLOR; int EntityID:ENTITYID; };
struct PSOutput { float4 Color:SV_TARGET0; int EntityID:SV_TARGET1; };
PSOutput PSMain(PSInput i) { PSOutput o; o.Color=i.Color; o.EntityID=i.EntityID; return o; }
)";
				auto blob = dev->CompileHLSL(linePSSrc, "PSMain", "ps_5_0", "Renderer2D_LinePS");
				if (blob)
					s_Data.D3D12LinePS = dev->CreateShaderModule(blob->GetBufferPointer(), static_cast<uint32_t>(blob->GetBufferSize()), "LinePS");
			}

			// --- Create textured root signature (shared by quad) ---
			s_Data.D3D12TexturedRootSig = dev->CreateTexturedRootSignature();

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

					if (s_Data.D3D12TexturedRootSig)
					{
						// Create pipeline directly with textured root signature
						s_Data.D3D12QuadPipeline = dev->CreateGraphicsPipelineWithRootSig(
							qd, s_Data.D3D12QuadVS, s_Data.D3D12QuadPS,
							s_Data.D3D12TexturedRootSig.Get());
					}
					else
					{
						s_Data.D3D12QuadPipeline = dev->CreateGraphicsPipeline(qd, s_Data.D3D12QuadVS, s_Data.D3D12QuadPS);
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

					s_Data.D3D12CirclePipeline = dev->CreateGraphicsPipeline(cd, s_Data.D3D12CircleVS, s_Data.D3D12CirclePS);
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

					s_Data.D3D12LinePipeline = dev->CreateGraphicsPipeline(ld, s_Data.D3D12LineVS, s_Data.D3D12LinePS);
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

		// No legacy UniformBuffer needed for D3D12; the per-frame camera CB
		// is updated inline via D3D12Buffer::Map/Unmap inside Flush().

		CANDY_CORE_INFO("Renderer2D: D3D12 backend initialized");
		return;
	}

		// =====================================================
		// OpenGL path — new RHI adapter (OpenGLRHIDevice/RHICommandBuffer)
		// =====================================================
		CANDY_CORE_INFO("Renderer2D: initializing OpenGL RHI backend...");

		auto* olDev = static_cast<OpenGLRHIDevice*>(RHIContext::GetDevice());
		if (!olDev)
		{
			CANDY_CORE_ERROR("Renderer2D: OpenGL API selected but no OpenGLRHIDevice published");
			s_Data.OL_Active = false;
			return;
		}

		// CPU-side vertex buffers (batched CPU writes each frame)
		s_Data.QuadVertexBufferBase   = new QuadVertex[s_Data.MaxVertices];
		s_Data.CircleVertexBufferBase = new CircleVertex[s_Data.MaxVertices];
		s_Data.LineVertexBufferBase   = new LineVertex[s_Data.MaxVertices];

		// GPU upload-heap vertex buffers (CPU-writable via Map/Unmap)
		auto makeUploadVB = [&](uint64_t size, uint32_t stride, const char* name) -> Ref<RHIBuffer> {
			BufferDesc d;
			d.Size          = size;
			d.Usage         = ResourceUsage::VertexBuffer;
			d.CPUAccessible = true;
			d.Stride        = stride;
			d.DebugName     = name;
			return olDev->CreateBuffer(d);
		};
		s_Data.OL_QuadVB   = makeUploadVB(s_Data.MaxVertices * sizeof(QuadVertex),   sizeof(QuadVertex),   "OL_QuadVB");
		s_Data.OL_CircleVB = makeUploadVB(s_Data.MaxVertices * sizeof(CircleVertex), sizeof(CircleVertex), "OL_CircleVB");
		s_Data.OL_LineVB   = makeUploadVB(s_Data.MaxVertices * sizeof(LineVertex),   sizeof(LineVertex),   "OL_LineVB");

		// Index buffer (quad/circle share)
		{
			uint32_t* quadIndices = new uint32_t[s_Data.MaxIndices];
			uint32_t off = 0;
			for (uint32_t i = 0; i < s_Data.MaxIndices; i += 6)
			{
				quadIndices[i + 0] = off + 0;
				quadIndices[i + 1] = off + 1;
				quadIndices[i + 2] = off + 2;
				quadIndices[i + 3] = off + 2;
				quadIndices[i + 4] = off + 3;
				quadIndices[i + 5] = off + 0;
				off += 4;
			}
			BufferDesc ib;
			ib.Size          = s_Data.MaxIndices * sizeof(uint32_t);
			ib.Usage         = ResourceUsage::IndexBuffer;
			ib.CPUAccessible = true;
			ib.DebugName     = "OL_QuadIB";
			s_Data.OL_QuadIB = olDev->CreateBuffer(ib);
			auto* rhiIB = dynamic_cast<OpenGLRHIBuffer*>(s_Data.OL_QuadIB.get());
			if (rhiIB)
			{
				void* m = rhiIB->Map();
				if (m) memcpy(m, quadIndices, static_cast<size_t>(ib.Size));
				rhiIB->Unmap();
			}
			delete[] quadIndices;
		}

		// Camera CB (UBO binding 0)
		{
			BufferDesc cb;
			cb.Size          = sizeof(Renderer2DData::CameraData);
			cb.Usage         = ResourceUsage::ConstantBuffer;
			cb.CPUAccessible = true;
			cb.DebugName     = "OL_CameraCB";
			s_Data.OL_CameraCB = olDev->CreateBuffer(cb);
		}

		// Shaders — load GLSL source via VFS, store as RHI source modules.
		auto loadShaderModule = [](const char* name, const char* vfsPath) -> Ref<RHIShaderModule> {
			auto src = FileSystem::Get().ReadText(vfsPath);
			if (!src)
			{
				CANDY_CORE_ERROR("Renderer2D (OpenGL): failed to load shader '{}'", vfsPath);
				return nullptr;
			}
			auto* dev = static_cast<OpenGLRHIDevice*>(RHIContext::GetDevice());
			return dev->CreateShaderModule(src->data(), static_cast<uint32_t>(src->size()), name);
		};
		s_Data.OL_QuadShader   = loadShaderModule("Renderer2D_Quad",   "VFS://Engine/Shaders/Renderer2D_Quad.glsl");
		s_Data.OL_CircleShader = loadShaderModule("Renderer2D_Circle", "VFS://Engine/Shaders/Renderer2D_Circle.glsl");
		s_Data.OL_LineShader   = loadShaderModule("Renderer2D_Line",   "VFS://Engine/Shaders/Renderer2D_Line.glsl");

		// Pipelines (mirrors D3D12 desc but routes through OpenGLRHIGraphicsPipeline)
		GraphicsPipelineDesc base;
		base.Topology                  = PrimitiveTopology::Triangles;
		base.Rasterizer.Cull           = CullMode::None;
		base.Rasterizer.Fill           = FillMode::Solid;
		base.DepthStencil.DepthTestEnable  = false;
		base.DepthStencil.DepthWriteEnable = false;
		base.Blend.BlendEnable         = true;
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
			s_Data.OL_QuadPipeline = olDev->CreateGraphicsPipeline(pd, s_Data.OL_QuadShader, s_Data.OL_QuadShader);
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
			s_Data.OL_CirclePipeline = olDev->CreateGraphicsPipeline(pd, s_Data.OL_CircleShader, s_Data.OL_CircleShader);
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
			s_Data.OL_LinePipeline = olDev->CreateGraphicsPipeline(pd, s_Data.OL_LineShader, s_Data.OL_LineShader);
		}

		// White texture (1x1 RGBA8) — use the legacy Texture2D factory which
		// returns an OpenGLTexture2D that double-inherits RHITexture.
		s_Data.WhiteTexture = Texture2D::Create(1, 1);
		uint32_t whiteTextureData = 0xffffffff;
		s_Data.WhiteTexture->SetData(&whiteTextureData, sizeof(uint32_t));
		s_Data.TextureSlots[0] = s_Data.WhiteTexture;

		s_Data.QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[2] = {  0.5f,  0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

		CANDY_CORE_INFO("Renderer2D: OpenGL RHI backend initialized");
	}

	void Renderer2D::Shutdown()
	{
		delete[] s_Data.QuadVertexBufferBase;
		if (s_Data.D3D12Active)
		{
			delete[] s_Data.CircleVertexBufferBase;
			delete[] s_Data.LineVertexBufferBase;
			s_Data.D3D12QuadVB.reset();
			s_Data.D3D12QuadIB.reset();
			s_Data.D3D12CircleVB.reset();
			s_Data.D3D12LineVB.reset();
			s_Data.D3D12CameraCB.reset();
			s_Data.D3D12QuadPipeline.reset();
			s_Data.D3D12CirclePipeline.reset();
			s_Data.D3D12LinePipeline.reset();
			s_Data.D3D12QuadVS.reset(); s_Data.D3D12QuadPS.reset();
			s_Data.D3D12CircleVS.reset(); s_Data.D3D12CirclePS.reset();
			s_Data.D3D12LineVS.reset(); s_Data.D3D12LinePS.reset();
			s_Data.D3D12TexturedRootSig.Reset();
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
		if (s_Data.D3D12Active || s_Data.VkActive || s_Data.OL_Active)
		{
			StartBatch();
			return;
		}

		s_Data.QuadShader->Bind();
		s_Data.QuadShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());

		StartBatch();
	}

	void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		s_Data.CameraBuffer.ViewProjection = camera.GetProjection() * glm::inverse(transform);
		if (!s_Data.D3D12Active && !s_Data.VkActive && !s_Data.OL_Active)
			s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer2DData::CameraData));

		StartBatch();
	}

	void Renderer2D::BeginScene(const EditorCamera& camera)
	{
		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjection();
		if (!s_Data.D3D12Active && !s_Data.VkActive && !s_Data.OL_Active)
			s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer2DData::CameraData));

		StartBatch();
	}

	void Renderer2D::EndScene()
	{
		if (!s_Data.D3D12Active && !s_Data.VkActive && !s_Data.OL_Active)
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
			rpDesc.ColorAttachments.push_back({RHIFormat::R8G8B8A8Unorm, LoadOp::Clear, StoreOp::Store, {0.1f,0.1f,0.15f,1.0f}});
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

		if (s_Data.D3D12Active)
		{
			// =====================================================
			// D3D12 path — upload vertex data + record draws
			// =====================================================
			auto* dev = s_Data.D3D12Dev;
			if (!dev) return;

			// Upload camera constant buffer
			{
				auto* d3d12CB = dynamic_cast<D3D12Buffer*>(s_Data.D3D12CameraCB.get());
				if (d3d12CB && d3d12CB->GetResource())
				{
					void* mapped = d3d12CB->Map();
					memcpy(mapped, &s_Data.CameraBuffer, sizeof(s_Data.CameraBuffer));
					d3d12CB->Unmap();
				}
			}

			auto& queue = dev->GetCommandQueue();
			auto  cmd   = queue.CreateCommandBuffer();
			if (!cmd) return;
			auto* d3d12cb = static_cast<D3D12CommandBuffer*>(cmd.get());

			// Set render target
			if (s_Data.ActiveRenderTarget)
			{
				d3d12cb->SetFramebufferRenderTarget(s_Data.ActiveRenderTarget);
			}
			else
			{
				// Fallback to swap chain (for non-viewport rendering)
				auto* gfxCtx = dynamic_cast<D3D12GraphicsContext*>(
					Application::Get().GetWindow().GetGraphicsContext());
				if (gfxCtx)
					d3d12cb->SetSwapChainRenderTarget(gfxCtx->GetSwapChain());
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
			if (s_Data.ActiveRenderTarget && s_Data.ActiveRenderTarget->GetColorAttachmentCount() > 1)
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
			if (s_Data.ActiveRenderTarget)
			{
				vpW = s_Data.ActiveRenderTarget->GetWidth();
				vpH = s_Data.ActiveRenderTarget->GetHeight();
			}
			cmd->SetViewport(0, 0, static_cast<float>(vpW), static_cast<float>(vpH));
			cmd->SetScissor(0, 0, vpW, vpH);

			// --- Quad batch (textured) ---
			if (s_Data.QuadIndexCount && s_Data.D3D12QuadPipeline)
			{
				uint32_t dataSize = static_cast<uint32_t>(
					reinterpret_cast<uint8_t*>(s_Data.QuadVertexBufferPtr) -
					reinterpret_cast<uint8_t*>(s_Data.QuadVertexBufferBase));

				auto* d3d12VB = dynamic_cast<D3D12Buffer*>(s_Data.D3D12QuadVB.get());
				if (d3d12VB)
				{
					void* mapped = d3d12VB->Map();
					memcpy(mapped, s_Data.QuadVertexBufferBase, dataSize);
					d3d12VB->Unmap();
				}

				cmd->SetPipeline(s_Data.D3D12QuadPipeline);
				cmd->SetConstantBuffer(0, 0, s_Data.D3D12CameraCB);
				cmd->SetVertexBuffer(s_Data.D3D12QuadVB);
				cmd->SetIndexBuffer(s_Data.D3D12QuadIB);

				// --- Bind textures via descriptor table ---
				ID3D12DescriptorHeap* srvHeap = dev->GetCBVSRVUAVHeap();
				uint32_t descSize = dev->GetCBVSRVDescriptorSize();
				if (srvHeap && s_Data.D3D12TexturedRootSig)
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

						auto* d3d12Tex = dynamic_cast<D3D12Texture2D*>(s_Data.TextureSlots[i].get());
						if (d3d12Tex && d3d12Tex->GetRHI() && d3d12Tex->GetRHI()->GetResource())
						{
							// Copy SRV descriptor from the texture's slot
							D3D12_CPU_DESCRIPTOR_HANDLE srcCPU = srvHeap->GetCPUDescriptorHandleForHeapStart();
							srcCPU.ptr += static_cast<SIZE_T>(d3d12Tex->GetRHI() ? 160 : 0) * descSize;

							// Rather than copy, just re-create the SRV at the target slot
							d3d12Tex->GetRHI()->CreateSRV(srvHeap, kBaseSlot + i, descSize);
						}
						else
						{
							// White texture fallback: use the first Texture2D (slot 0)
							auto* whiteTex = dynamic_cast<D3D12Texture2D*>(s_Data.TextureSlots[0].get());
							if (whiteTex && whiteTex->GetRHI())
								whiteTex->GetRHI()->CreateSRV(srvHeap, kBaseSlot + i, descSize);
						}
					}

					// Bind descriptor table (root parameter 1)
					D3D12_GPU_DESCRIPTOR_HANDLE gpuTable = srvHeap->GetGPUDescriptorHandleForHeapStart();
					gpuTable.ptr += static_cast<SIZE_T>(kBaseSlot) * descSize;
					d3d12cb->GetNativeCommandList()->SetGraphicsRootDescriptorTable(1, gpuTable);
				}

				cmd->DrawIndexed(s_Data.QuadIndexCount);
				s_Data.Stats.DrawCalls++;
			}

			// --- Circle batch ---
			if (s_Data.CircleIndexCount && s_Data.D3D12CirclePipeline)
			{
				uint32_t dataSize = static_cast<uint32_t>(
					reinterpret_cast<uint8_t*>(s_Data.CircleVertexBufferPtr) -
					reinterpret_cast<uint8_t*>(s_Data.CircleVertexBufferBase));

				auto* d3d12VB = dynamic_cast<D3D12Buffer*>(s_Data.D3D12CircleVB.get());
				if (d3d12VB)
				{
					void* mapped = d3d12VB->Map();
					memcpy(mapped, s_Data.CircleVertexBufferBase, dataSize);
					d3d12VB->Unmap();
				}

				cmd->SetPipeline(s_Data.D3D12CirclePipeline);
				cmd->SetConstantBuffer(0, 0, s_Data.D3D12CameraCB);
				cmd->SetVertexBuffer(s_Data.D3D12CircleVB);
				cmd->SetIndexBuffer(s_Data.D3D12QuadIB); // reuse quad IB
				cmd->DrawIndexed(s_Data.CircleIndexCount);
				s_Data.Stats.DrawCalls++;
			}

			// --- Line batch ---
			if (s_Data.LineVertexCount && s_Data.D3D12LinePipeline)
			{
				uint32_t dataSize = static_cast<uint32_t>(
					reinterpret_cast<uint8_t*>(s_Data.LineVertexBufferPtr) -
					reinterpret_cast<uint8_t*>(s_Data.LineVertexBufferBase));

				auto* d3d12VB = dynamic_cast<D3D12Buffer*>(s_Data.D3D12LineVB.get());
				if (d3d12VB)
				{
					void* mapped = d3d12VB->Map();
					memcpy(mapped, s_Data.LineVertexBufferBase, dataSize);
					d3d12VB->Unmap();
				}

				cmd->SetPipeline(s_Data.D3D12LinePipeline);
				cmd->SetConstantBuffer(0, 0, s_Data.D3D12CameraCB);
				cmd->SetVertexBuffer(s_Data.D3D12LineVB);
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
		// OpenGL RHI path — record via OpenGLRHICommandBuffer
		// =====================================================
		{
			auto* olDev = static_cast<OpenGLRHIDevice*>(RHIContext::GetDevice());
			if (!olDev)
			{
				CANDY_CORE_ERROR("Renderer2D::Flush (OpenGL): no OpenGLRHIDevice");
				return;
			}

			auto& queue = olDev->GetCommandQueue();
			auto  cmd   = queue.CreateCommandBuffer(); // Scope<RHICommandBuffer>
			auto* gl    = static_cast<OpenGLRHICommandBuffer*>(cmd.get());

			// Pick the render target — EditorLayer hands in a viewport
			// OpenGLFramebuffer via SetActiveRenderTarget; fall back to the
			// swap chain (default framebuffer) when rendering the title-bar area.
			uint32_t vpW = 1280, vpH = 720;
			if (s_Data.ActiveRenderTarget)
			{
				gl->SetFramebufferRenderTarget(s_Data.ActiveRenderTarget);
				vpW = s_Data.ActiveRenderTarget->GetWidth();
				vpH = s_Data.ActiveRenderTarget->GetHeight();
			}
			else
			{
				gl->SetSwapChainRenderTarget(RHIContext::GetSwapChain());
			}

			gl->Begin();

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
			}
			gl->BeginRenderPass(rpDesc);
			gl->SetViewport(0.0f, 0.0f, static_cast<float>(vpW), static_cast<float>(vpH));
			gl->SetScissor(0, 0, vpW, vpH);

			// Upload camera CB
			if (auto* cb = dynamic_cast<OpenGLRHIBuffer*>(s_Data.OL_CameraCB.get()))
			{
				void* m = cb->Map();
				if (m) memcpy(m, &s_Data.CameraBuffer, sizeof(Renderer2DData::CameraData));
				cb->Unmap();
			}

			// Quad batch (textured)
			if (s_Data.QuadIndexCount && s_Data.OL_QuadPipeline)
			{
				uint32_t dataSize = static_cast<uint32_t>(
				    reinterpret_cast<uint8_t*>(s_Data.QuadVertexBufferPtr) -
				    reinterpret_cast<uint8_t*>(s_Data.QuadVertexBufferBase));
				if (auto* vb = dynamic_cast<OpenGLRHIBuffer*>(s_Data.OL_QuadVB.get()))
				{
					void* m = vb->Map();
					if (m) memcpy(m, s_Data.QuadVertexBufferBase, dataSize);
					vb->Unmap();
				}

				gl->SetPipeline(s_Data.OL_QuadPipeline);
				gl->SetConstantBuffer(0, 0, s_Data.OL_CameraCB);
				gl->SetVertexBuffer(s_Data.OL_QuadVB, 0, 0);
				gl->SetIndexBuffer(s_Data.OL_QuadIB, IndexFormat::UInt32, 0);

				// Bind used textures (TextureSlots[i] are OpenGLTexture2D which
				// double-inherit RHITexture), unfilled slots fall back to white.
				for (uint32_t i = 0; i < s_Data.MaxTextureSlots; ++i)
				{
					Ref<Texture2D> src = (i < s_Data.TextureSlotIndex) ? s_Data.TextureSlots[i] : s_Data.TextureSlots[0];
					if (auto rhiTex = std::dynamic_pointer_cast<RHITexture>(src))
						gl->SetTexture(1, i, rhiTex);
				}

				gl->DrawIndexed(s_Data.QuadIndexCount, 1, 0, 0, 0);
				s_Data.Stats.DrawCalls++;
			}

			// Circle batch
			if (s_Data.CircleIndexCount && s_Data.OL_CirclePipeline)
			{
				uint32_t dataSize = static_cast<uint32_t>(
				    reinterpret_cast<uint8_t*>(s_Data.CircleVertexBufferPtr) -
				    reinterpret_cast<uint8_t*>(s_Data.CircleVertexBufferBase));
				if (auto* vb = dynamic_cast<OpenGLRHIBuffer*>(s_Data.OL_CircleVB.get()))
				{
					void* m = vb->Map();
					if (m) memcpy(m, s_Data.CircleVertexBufferBase, dataSize);
					vb->Unmap();
				}

				gl->SetPipeline(s_Data.OL_CirclePipeline);
				gl->SetConstantBuffer(0, 0, s_Data.OL_CameraCB);
				gl->SetVertexBuffer(s_Data.OL_CircleVB, 0, 0);
				gl->SetIndexBuffer(s_Data.OL_QuadIB, IndexFormat::UInt32, 0);
				gl->DrawIndexed(s_Data.CircleIndexCount, 1, 0, 0, 0);
				s_Data.Stats.DrawCalls++;
			}

			// Line batch
			if (s_Data.LineVertexCount && s_Data.OL_LinePipeline)
			{
				uint32_t dataSize = static_cast<uint32_t>(
				    reinterpret_cast<uint8_t*>(s_Data.LineVertexBufferPtr) -
				    reinterpret_cast<uint8_t*>(s_Data.LineVertexBufferBase));
				if (auto* vb = dynamic_cast<OpenGLRHIBuffer*>(s_Data.OL_LineVB.get()))
				{
					void* m = vb->Map();
					if (m) memcpy(m, s_Data.LineVertexBufferBase, dataSize);
					vb->Unmap();
				}

				gl->SetPipeline(s_Data.OL_LinePipeline);
				gl->SetConstantBuffer(0, 0, s_Data.OL_CameraCB);
				gl->SetVertexBuffer(s_Data.OL_LineVB, 0, 0);
				gl->Draw(s_Data.LineVertexCount, 1, 0, 0);
				s_Data.Stats.DrawCalls++;
			}

			gl->EndRenderPass();
			gl->End();
			queue.Submit({ cmd.get() });
			olDev->WaitIdle();
			return;
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

	void Renderer2D::SetActiveRenderTarget(const Ref<RHIFramebuffer>& fb)
	{
		s_Data.ActiveRenderTarget = fb;
	}
}

