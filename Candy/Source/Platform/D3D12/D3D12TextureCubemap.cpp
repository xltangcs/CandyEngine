#include "CandyPCH.h"
#include <Windows.h>

#include "Platform/D3D12/D3D12TextureCubemap.h"
#include "Platform/D3D12/D3D12Device.h"
#include "Platform/D3D12/D3D12Texture.h"
#include "Platform/D3D12/D3D12PipelineState.h"
#include "Platform/D3D12/D3D12CommandBuffer.h"
#include "Runtime/Core/FileSystem.h"
#include "Runtime/Core/Log.h"
#include "Runtime/RHI/RHICommandQueue.h"
#include "Runtime/RHI/RHICommandBuffer.h"

#include <stb_image.h>

#include <chrono>

namespace Candy {

	namespace {

		constexpr const char* kBakeShaderPath = "VFS://Engine/Content/Shaders/D3D12/IBLBake.hlsl";

		// ---- Bake parameters (byte-for-byte with IBLBake.hlsl BakeCB) --------
		struct BakeCB
		{
			float    FaceSize    = 0.0f;  // cubemap face size for this dispatch (float in HLSL!)
			uint32_t MipIndex    = 0;     // output mip level (mip chain / prefilter)
			float    Roughness   = 0.0f;  // prefilter roughness (0..1)
			uint32_t SampleCount = 0;     // Monte-Carlo samples per texel
			float    _Pad[4]     = { 0.0f, 0.0f, 0.0f, 0.0f };
		};
		static_assert(sizeof(BakeCB) == 32, "BakeCB must match IBLBake.hlsl (32B)");

		// ---- Shared compute pipelines (created once, cached for the session) --
		struct BakePipelines
		{
			Ref<RHIComputePipeline> EquirectToCube;
			Ref<RHIComputePipeline> CubeMipChain;
			Ref<RHIComputePipeline> Irradiance;
			Ref<RHIComputePipeline> Prefilter;
			Ref<RHIComputePipeline> BRDFLUT;
		};

		const BakePipelines& GetBakePipelines(D3D12Device* device)
		{
			static BakePipelines s_Pipelines;
			static bool s_Ready = false;
			if (s_Ready)
				return s_Pipelines;

			auto load = [&](const char* entry, const char* name) -> Ref<RHIComputePipeline> {
				auto src = FileSystem::Get().ReadText(kBakeShaderPath);
				if (!src)
				{
					CANDY_CORE_ERROR("D3D12TextureCubemap: failed to load bake shader '{}'", kBakeShaderPath);
					return nullptr;
				}
				auto cs = device->CreateShaderModuleFromSource(src->c_str(), ShaderStage::Compute, entry, name);
				if (!cs)
					return nullptr;
				return device->CreateComputePipeline(cs);
			};

			s_Pipelines.EquirectToCube = load("EquirectToCubeCS", "IBLBake_EquirectToCube");
			s_Pipelines.CubeMipChain   = load("CubeMipChainCS",   "IBLBake_CubeMipChain");
			s_Pipelines.Irradiance     = load("IrradianceCS",     "IBLBake_Irradiance");
			s_Pipelines.Prefilter      = load("PrefilterCS",      "IBLBake_Prefilter");
			s_Pipelines.BRDFLUT        = load("BRDFLUTCS",        "IBLBake_BRDFLUT");

			s_Ready = s_Pipelines.EquirectToCube && s_Pipelines.CubeMipChain
				&& s_Pipelines.Irradiance && s_Pipelines.Prefilter && s_Pipelines.BRDFLUT;
			return s_Pipelines;
		}

		// Load an image as float RGBA (HDR .hdr via stbi_loadf, LDR via
		// stbi_load converted to float). Returns false on failure.
		bool LoadFloatRGBA(const std::string& vfsPath, std::vector<float>& rgba,
		                   uint32_t& width, uint32_t& height)
		{
			int w = 0, h = 0, comp = 0;
			stbi_uc* ldr = nullptr;
			float* hdr = nullptr;

			if (FileSystem::Get().Exists(vfsPath))
			{
				auto fileData = FileSystem::Get().Read(vfsPath);
				if (!fileData)
					return false;

				hdr = stbi_loadf_from_memory(
					reinterpret_cast<const stbi_uc*>(fileData->data()),
					static_cast<int>(fileData->size()),
					&w, &h, &comp, 4);
				if (!hdr)
				{
					ldr = stbi_load_from_memory(
						reinterpret_cast<const stbi_uc*>(fileData->data()),
						static_cast<int>(fileData->size()),
						&w, &h, &comp, 4);
				}
			}
			else
			{
				hdr = stbi_loadf(vfsPath.c_str(), &w, &h, &comp, 4);
				if (!hdr)
					ldr = stbi_load(vfsPath.c_str(), &w, &h, &comp, 4);
			}

			if (!hdr && !ldr)
			{
				CANDY_CORE_ERROR("D3D12TextureCubemap: failed to load equirect panorama '{}'", vfsPath);
				return false;
			}

			width  = static_cast<uint32_t>(w);
			height = static_cast<uint32_t>(h);
			rgba.resize(static_cast<size_t>(w) * h * 4);

			if (hdr)
			{
				std::memcpy(rgba.data(), hdr, rgba.size() * sizeof(float));
				stbi_image_free(hdr);
			}
			else
			{
				for (size_t i = 0; i < static_cast<size_t>(w) * h * 4; ++i)
					rgba[i] = static_cast<float>(ldr[i]) / 255.0f;
				stbi_image_free(ldr);
			}
			return true;
		}

	}

	D3D12TextureCubemap::D3D12TextureCubemap(D3D12Device* device, const std::string& vfsPath)
		: m_Device(device), m_Path(vfsPath)
	{
		if (!device)
			return;

		std::vector<float> rgba;
		uint32_t w = 0, h = 0;
		if (!LoadFloatRGBA(vfsPath, rgba, w, h))
			return;

		if (!BakeAndUpload(rgba.data(), w, h))
			return;

		m_IsLoaded = true;
		CANDY_CORE_INFO("D3D12TextureCubemap: loaded '{}' ({}x{} equirect, GPU-baked)", vfsPath, w, h);
	}

	bool D3D12TextureCubemap::BakeAndUpload(const float* rgba, uint32_t width, uint32_t height)
	{
		auto start = std::chrono::steady_clock::now();
		auto* dev = m_Device;

		constexpr uint32_t kEnvSize  = 256; // skybox cubemap face size
		constexpr uint32_t kEnvMips  = 9;   // 256 -> 1
		constexpr uint32_t kIrrSize  = 32;
		constexpr uint32_t kPrefSize = 128;
		constexpr uint32_t kPrefMips = 6;
		constexpr uint32_t kLUTSize  = 256;

		// ---- Equirect 2D texture (uploaded via the plain 2D path) ----------
		TextureDesc eqDesc;
		eqDesc.Width     = width;
		eqDesc.Height    = height;
		eqDesc.Format    = RHIFormat::R32G32B32A32Float;
		eqDesc.Usage     = ResourceUsage::ShaderRead | ResourceUsage::CopyDst;
		eqDesc.DebugName = "IBLBake_Equirect";
		Ref<RHITexture> equirect = dev->CreateTexture(eqDesc);
		if (!equirect)
			return false;
		auto* eqTex = dynamic_cast<D3D12Texture*>(equirect.get());
		eqTex->SetData(rgba, width * 4 * sizeof(float));

		// ---- Output textures (UAV-capable) ----------------------------------
		auto makeCube = [&](uint32_t size, uint32_t mips, const char* name) -> Ref<RHITexture> {
			TextureDesc desc;
			desc.Width     = size;
			desc.Height    = size;
			desc.MipLevels = mips;
			desc.Format    = RHIFormat::R32G32B32A32Float;
			desc.Usage     = ResourceUsage::ShaderRead | ResourceUsage::ShaderWrite | ResourceUsage::CopyDst;
			desc.Type      = TextureType::Cubemap;
			desc.DebugName = name;
			return dev->CreateTexture(desc);
		};

		m_Environment = makeCube(kEnvSize, kEnvMips, "SkyboxEnv");
		m_Irradiance  = makeCube(kIrrSize, 1, "SkyboxIrradiance");
		m_Prefiltered = makeCube(kPrefSize, kPrefMips, "SkyboxPrefiltered");

		{
			TextureDesc lutDesc;
			lutDesc.Width     = kLUTSize;
			lutDesc.Height    = kLUTSize;
			lutDesc.Format    = RHIFormat::R32G32Float;
			lutDesc.Usage     = ResourceUsage::ShaderRead | ResourceUsage::ShaderWrite | ResourceUsage::CopyDst;
			lutDesc.DebugName = "SkyboxBRDFLUT";
			m_BRDFLUT = dev->CreateTexture(lutDesc);
		}

		if (!m_Environment || !m_Irradiance || !m_Prefiltered || !m_BRDFLUT)
			return false;

		auto* envTex  = dynamic_cast<D3D12Texture*>(m_Environment.get());
		auto* irrTex  = dynamic_cast<D3D12Texture*>(m_Irradiance.get());
		auto* prefTex = dynamic_cast<D3D12Texture*>(m_Prefiltered.get());
		auto* lutTex  = dynamic_cast<D3D12Texture*>(m_BRDFLUT.get());

		// ---- Bake pipelines -------------------------------------------------
		const BakePipelines& p = GetBakePipelines(dev);
		if (!p.EquirectToCube || !p.CubeMipChain || !p.Irradiance || !p.Prefilter || !p.BRDFLUT)
			return false;

		// ---- Bake constant buffer: one 256B-aligned slice PER DISPATCH ------
		// Recording happens before submission, so every dispatch needs its own
		// params — a shared address would make all dispatches read the last
		// Write at GPU execution time (same rule as SceneRenderer's MaterialCB).
		constexpr uint32_t kMaxDispatches = 32;
		BufferDesc cbDesc;
		cbDesc.Size          = static_cast<uint64_t>(kMaxDispatches) * 256;
		cbDesc.Usage         = ResourceUsage::ConstantBuffer;
		cbDesc.CPUAccessible = true;
		cbDesc.DebugName     = "IBLBake_CB";
		Ref<RHIBuffer> bakeCB = dev->CreateBuffer(cbDesc);
		if (!bakeCB)
			return false;

		uint32_t cbSlice = 0;
		auto setParams = [&](const BakeCB& p) -> uint64_t {
			const uint64_t offset = static_cast<uint64_t>(cbSlice++) * 256;
			bakeCB->Write(&p, sizeof(p), offset);
			return offset;
		};

		// ---- Record the full bake on one command list ----------------------
		auto& queue = dev->GetCommandQueue();
		auto cmd = queue.CreateCommandBuffer();
		if (!cmd)
			return false;

		// D3D12-only baking: transitions need the native command list.
		auto* d3d12cmd = static_cast<D3D12CommandBuffer*>(cmd.get());
		auto* list = d3d12cmd->GetNativeCommandList();

		cmd->Begin();

		BakeCB params;

		// Transition inputs/outputs: equirect is PIXEL_SHADER_RESOURCE after
		// SetData; compute reads need NON_PIXEL_SHADER_RESOURCE. Outputs start
		// in COMMON and get UAV transitions per pass.
		eqTex->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		// D3D12 subresource = mip + face * mipCount (array-major). Cubemap
		// state transitions must cover all 6 faces of a mip level.
		auto transitionCubeMip = [](D3D12Texture* tex, ID3D12GraphicsCommandList* l,
		                            uint32_t mip, uint32_t mipCount,
		                            D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
			for (uint32_t f = 0; f < 6; ++f)
				tex->TransitionSubresource(l, mip + f * mipCount, from, to);
		};

		// ---- Pass 1: equirect -> env cube mip0 ------------------------------
		envTex->Transition(list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		{
			params.FaceSize = static_cast<float>(kEnvSize);
			params.MipIndex = 0;
			cmd->SetComputePipeline(p.EquirectToCube);
			cmd->SetComputeConstantBuffer(0, bakeCB, setParams(params));
			Ref<RHITexture> src = equirect;
			cmd->SetComputeTextures(1, 1, &src);
			Ref<RHITexture> dst = m_Environment;
			cmd->SetComputeUAVs(2, 1, &dst, 0); // mip 0
			cmd->Dispatch(kEnvSize / 8, kEnvSize / 8, 6);
			cmd->UAVBarrier();
		}

		// ---- Pass 2: env mip chain (each mip reads the previous via SRV) ----
		{
			cmd->SetComputePipeline(p.CubeMipChain);
			Ref<RHITexture> src = m_Environment;
			Ref<RHITexture> dst = m_Environment;
			for (uint32_t mip = 1; mip < kEnvMips; ++mip)
			{
				const uint32_t size = kEnvSize >> mip;

				// Parent mip becomes readable (SRV); the target mip was already
				// transitioned to UAV by the initial whole-resource transition.
				transitionCubeMip(envTex, list, mip - 1, kEnvMips,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

				params.FaceSize = static_cast<float>(size);
				params.MipIndex = mip; // CubeMipChainCS samples mip - 1 internally
				cmd->SetComputeConstantBuffer(0, bakeCB, setParams(params));
				cmd->SetComputeTextures(1, 1, &src);
				cmd->SetComputeUAVs(2, 1, &dst, mip);
				cmd->Dispatch((size + 7) / 8, (size + 7) / 8, 6);
				cmd->UAVBarrier();
			}
			// Last env mip (still UAV) becomes readable too.
			transitionCubeMip(envTex, list, kEnvMips - 1, kEnvMips,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		}

		// ---- Pass 3: irradiance (reads env mip0, writes 32px cube) ----------
		{
			params.FaceSize    = static_cast<float>(kIrrSize);
			params.SampleCount = 1024;
			irrTex->Transition(list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			cmd->SetComputePipeline(p.Irradiance);
			cmd->SetComputeConstantBuffer(0, bakeCB, setParams(params));
			Ref<RHITexture> src = m_Environment;
			cmd->SetComputeTextures(1, 1, &src);
			Ref<RHITexture> dst = m_Irradiance;
			cmd->SetComputeUAVs(2, 1, &dst, 0);
			cmd->Dispatch(kIrrSize / 8, kIrrSize / 8, 6);
			cmd->UAVBarrier();
		}

		// ---- Pass 4: prefiltered specular (one dispatch per roughness mip) --
		{
			cmd->SetComputePipeline(p.Prefilter);
			Ref<RHITexture> src = m_Environment;
			Ref<RHITexture> dst = m_Prefiltered;
			for (uint32_t mip = 0; mip < kPrefMips; ++mip)
			{
				const uint32_t size = kPrefSize >> mip;
				params.FaceSize    = static_cast<float>(size);
				params.MipIndex    = mip;
				params.Roughness   = static_cast<float>(mip) / static_cast<float>(kPrefMips - 1);
				params.SampleCount = 256;

				// Target mip transitions COMMON -> UAV (per-mip so the
				// whole-resource tracked state stays valid for the final
				// transition), then back to readable after the dispatch.
				transitionCubeMip(prefTex, list, mip, kPrefMips,
					D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

				cmd->SetComputeConstantBuffer(0, bakeCB, setParams(params));
				cmd->SetComputeTextures(1, 1, &src);
				cmd->SetComputeUAVs(2, 1, &dst, mip);
				cmd->Dispatch((size + 7) / 8, (size + 7) / 8, 6);
				cmd->UAVBarrier();

				transitionCubeMip(prefTex, list, mip, kPrefMips,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			}
		}

		// ---- Pass 5: BRDF LUT (2D, fixed function) ---------------------------
		{
			params.FaceSize    = static_cast<float>(kLUTSize);
			params.SampleCount = 1024;
			lutTex->Transition(list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			cmd->SetComputePipeline(p.BRDFLUT);
			cmd->SetComputeConstantBuffer(0, bakeCB, setParams(params));
			Ref<RHITexture> dst = m_BRDFLUT;
			cmd->SetComputeUAVs(2, 1, &dst, 0);
			cmd->Dispatch(kLUTSize / 8, kLUTSize / 8, 1);
			cmd->UAVBarrier();
		}

		// ---- Final transitions: everything back to shader-read --------------
		// env/prefiltered are fully ALL_SHADER_RESOURCE after their mip loops
		// (mixed subresource transitions handled above); just sync tracked state.
		envTex->SetState(D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		prefTex->SetState(D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		irrTex->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		lutTex->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		eqTex->Transition(list, D3D12_RESOURCE_STATE_COMMON);

		cmd->End();
		queue.Submit({ cmd.get() });
		dev->WaitIdle();

		m_FaceSize  = kEnvSize;
		m_MipLevels = kEnvMips;

		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - start).count();
		CANDY_CORE_INFO("D3D12TextureCubemap: GPU bake took {} ms ('{}')", elapsed, m_Path);

		// Equirect staging texture is no longer needed after baking.
		equirect.reset();

		return true;
	}

} // namespace Candy
