#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/RHI/RHIDevice.h"

#include <string>

namespace Candy {

	// =========================================================================
	// TextureCubemap — engine-level cubemap resource built from an
	// equirectangular panorama, with the full split-sum IBL set:
	//   - the environment cubemap itself (skybox sampling, mip chain)
	//   - irradiance cubemap     (diffuse IBL)
	//   - prefiltered cubemap    (specular IBL, per-roughness mips)
	//   - BRDF LUT               (2D, split-sum scale/bias)
	//
	// Baking happens once per asset on the GPU via compute shaders
	// (VFS://Engine/Content/Shaders/D3D12/IBLBake.hlsl); results are cached by
	// VFS path. D3D12 is the only supported backend for now (SceneRenderer is
	// D3D12-only); other backends return nullptr with a warning.
	// =========================================================================
	class TextureCubemap : public std::enable_shared_from_this<TextureCubemap>
	{
	public:
		virtual ~TextureCubemap() = default;

		/// Load + bake from an equirectangular panorama (HDR .hdr or LDR image).
		/// Cached by VFS path.
		static Ref<TextureCubemap> CreateFromEquirect(const std::string& vfsPath);

		virtual bool IsLoaded() const = 0;
		virtual uint32_t GetFaceSize() const = 0;
		virtual uint32_t GetMipLevels() const = 0;

		virtual Ref<RHITexture> GetRHITexture() = 0;       // environment cube
		virtual Ref<RHITexture> GetIrradianceMap() = 0;    // diffuse cube
		virtual Ref<RHITexture> GetPrefilteredMap() = 0;   // specular cube (mips)
		virtual Ref<RHITexture> GetBRDFLUT() = 0;          // 2D RG
	};
}
