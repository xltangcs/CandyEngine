#pragma once

#include "Runtime/Renderer/TextureCubemap.h"

namespace Candy {

	class D3D12Device;

	/// D3D12 backend factory — registered by D3D12GraphicsContext::Init via
	/// TextureCubemap::RegisterBackendFactory (composition root).
	Ref<TextureCubemap> CreateD3D12TextureCubemapFromEquirect(const std::string& vfsPath);

	// =========================================================================
	// D3D12TextureCubemap — D3D12 implementation of the cubemap + IBL set.
	// Loads the equirect panorama and bakes everything on the GPU with
	// compute shaders (IBLBake.hlsl):
	//   - environment cube (R32G32B32A32Float, full mip chain)
	//   - irradiance cube  (R32G32B32A32Float, 32px faces)
	//   - prefiltered cube (R32G32B32A32Float, per-roughness mips)
	//   - BRDF LUT 2D      (R32G32Float, 256x256)
	// =========================================================================
	class D3D12TextureCubemap : public TextureCubemap
	{
	public:
		D3D12TextureCubemap(D3D12Device* device, const std::string& vfsPath);
		~D3D12TextureCubemap() override = default;

		bool IsLoaded() const override { return m_IsLoaded; }
		uint32_t GetFaceSize() const override { return m_FaceSize; }
		uint32_t GetMipLevels() const override { return m_MipLevels; }

		Ref<RHITexture> GetRHITexture() override { return m_Environment; }
		Ref<RHITexture> GetIrradianceMap() override { return m_Irradiance; }
		Ref<RHITexture> GetPrefilteredMap() override { return m_Prefiltered; }
		Ref<RHITexture> GetBRDFLUT() override { return m_BRDFLUT; }

	private:
		bool BakeAndUpload(const float* rgba, uint32_t width, uint32_t height);

		D3D12Device* m_Device = nullptr;
		std::string  m_Path;
		uint32_t     m_FaceSize = 0;
		uint32_t     m_MipLevels = 0;
		bool         m_IsLoaded = false;

		Ref<RHITexture> m_Environment;
		Ref<RHITexture> m_Irradiance;
		Ref<RHITexture> m_Prefiltered;
		Ref<RHITexture> m_BRDFLUT;
	};

} // namespace Candy
