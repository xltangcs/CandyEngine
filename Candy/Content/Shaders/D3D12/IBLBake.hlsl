// CandyEngine IBL baking compute shaders (D3D12, SM5.0).
//
// Five entry points, all sharing one compute root signature
// (D3D12Device::CreateComputeRootSignature):
//   BakeCB (b0): u_FaceSize (current mip size), u_MipIndex, u_Roughness,
//                u_SampleCount
//   SRV    (t0): source texture — equirect 2D (EquirectToCubeCS) or env
//                cubemap (CubeMipChain/Irradiance/Prefilter)
//   UAV    (u0): output — cube faces as Texture2DArray (x,y,face) or the
//                BRDF LUT as Texture2D
//
// Output texture formats: R32G32B32A32Float cubes (env / irradiance /
// prefiltered) and R32G32Float BRDF LUT. Cubemap UAVs are addressed as
// Texture2DArray (D3D12 has no TextureCube UAV dimension); the SRV views
// created afterwards use TextureCube.

cbuffer BakeCB : register(b0)
{
	float  u_FaceSize;    // current mip face size (pixels)
	uint   u_MipIndex;    // output mip level (0 = base)
	float  u_Roughness;   // prefilter roughness (0..1)
	uint   u_SampleCount; // Monte-Carlo samples per texel
};

// ---- Cubemap conventions (matches CPU reference + TextureCube sampling) ----
// +X right, -X left, +Y up, -Y down, +Z front, -Z back.

static const float3 kFaceDirs[6] = {
	float3( 1.0,  0.0,  0.0), // +X
	float3(-1.0,  0.0,  0.0), // -X
	float3( 0.0,  1.0,  0.0), // +Y
	float3( 0.0, -1.0,  0.0), // -Y
	float3( 0.0,  0.0,  1.0), // +Z
	float3( 0.0,  0.0, -1.0), // -Z
};

static const float3 kFaceUp[6] = {
	float3(0.0,  1.0,  0.0), // +X
	float3(0.0,  1.0,  0.0), // -X
	float3(0.0,  0.0, -1.0), // +Y
	float3(0.0,  0.0,  1.0), // -Y
	float3(0.0,  1.0,  0.0), // +Z
	float3(0.0,  1.0,  0.0), // -Z
};

static const float kPI = 3.14159265358979;

// texel (x, y) on face f -> direction vector
float3 FaceTexelToDir(uint face, uint x, uint y)
{
	float u = (x + 0.5) / u_FaceSize * 2.0 - 1.0;
	float v = (y + 0.5) / u_FaceSize * 2.0 - 1.0;
	return normalize(kFaceDirs[face] + kFaceUp[face] * v
	                 + cross(kFaceUp[face], kFaceDirs[face]) * u);
}

// ---- Quasi-random helpers (same as the CPU baker) ---------------------------

float RadicalInverseVdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10;
}

float2 Hammersley(uint i, uint n)
{
	return float2(float(i) / float(n), RadicalInverseVdC(i));
}

float3 ImportanceSampleGGX(float2 xi, float3 n, float roughness)
{
	float a = roughness * roughness;
	float phi = 2.0 * kPI * xi.x;
	float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
	float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));

	float3 h = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

	float3 up = abs(n.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
	float3 t = normalize(cross(up, n));
	float3 b = cross(n, t);

	return normalize(t * h.x + b * h.y + n * h.z);
}

// ---- Pass 1: equirectangular -> cubemap base mip ----------------------------
// Dispatch: (FaceSize/8, FaceSize/8, 6)

Texture2D<float4> u_Equirect : register(t0);
RWTexture2DArray<float4> u_CubeOut : register(u0);
SamplerState u_Sampler : register(s0);

[numthreads(8, 8, 1)]
void EquirectToCubeCS(uint3 id : SV_DispatchThreadID)
{
	float3 dir = FaceTexelToDir(id.z, id.x, id.y);

	float u = atan2(dir.z, dir.x) * (0.5 / kPI) + 0.5;
	float v = acos(clamp(dir.y, -1.0, 1.0)) / kPI;

	u_CubeOut[uint3(id.x, id.y, id.z)] = u_Equirect.SampleLevel(u_Sampler, float2(u, v), 0.0);
}

// ---- Pass 2: box-filtered mip chain -----------------------------------------
// Dispatch: (FaceSize/8, FaceSize/8, 6) for each mip >= 1.

TextureCube<float4> u_EnvCube : register(t0);

[numthreads(8, 8, 1)]
void CubeMipChainCS(uint3 id : SV_DispatchThreadID)
{
	float3 dir = FaceTexelToDir(id.z, id.x, id.y);

	// Sample the previous mip at the same direction; hardware cube filtering
	// handles face seams.
	u_CubeOut[uint3(id.x, id.y, id.z)] =
		u_EnvCube.SampleLevel(u_Sampler, dir, float(u_MipIndex - 1));
}

// ---- Pass 3: irradiance (diffuse) convolution --------------------------------
// Dispatch: (32/8, 32/8, 6). Cosine-weighted hemisphere, Hammersley samples.

[numthreads(8, 8, 1)]
void IrradianceCS(uint3 id : SV_DispatchThreadID)
{
	float3 n = FaceTexelToDir(id.z, id.x, id.y);

	float3 up = abs(n.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
	float3 t = normalize(cross(up, n));
	float3 b = cross(n, t);

	float3 irradiance = 0.0;
	for (uint i = 0; i < u_SampleCount; ++i)
	{
		float2 xi = Hammersley(i, u_SampleCount);
		float phi = 2.0 * kPI * xi.x;
		float cosTheta = sqrt(xi.y);
		float sinTheta = sqrt(max(0.0, 1.0 - xi.y));
		float3 local = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
		float3 dir = normalize(t * local.x + b * local.y + n * local.z);
		irradiance += u_EnvCube.SampleLevel(u_Sampler, dir, 0.0).rgb;
	}

	u_CubeOut[uint3(id.x, id.y, id.z)] =
		float4(irradiance * (kPI / float(u_SampleCount)), 1.0);
}

// ---- Pass 4: prefiltered specular (GGX importance sampling) ------------------
// Dispatch: (FaceSize/8, FaceSize/8, 6) for each mip; roughness = mip/(mips-1).

[numthreads(8, 8, 1)]
void PrefilterCS(uint3 id : SV_DispatchThreadID)
{
	float3 n = FaceTexelToDir(id.z, id.x, id.y);
	float3 v = n; // prefilter integrates over the reflection lobe

	float3 up = abs(n.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
	float3 t = normalize(cross(up, n));
	float3 b = cross(n, t);

	float3 total = 0.0;
	float weight = 0.0;
	for (uint i = 0; i < u_SampleCount; ++i)
	{
		float2 xi = Hammersley(i, u_SampleCount);
		float3 h = ImportanceSampleGGX(xi, n, u_Roughness);
		float3 l = normalize(2.0 * dot(v, h) * h - v);

		float ndotl = dot(n, l);
		if (ndotl > 0.0)
		{
			total += u_EnvCube.SampleLevel(u_Sampler, l, 0.0).rgb * ndotl;
			weight += ndotl;
		}
	}

	u_CubeOut[uint3(id.x, id.y, id.z)] =
		float4(weight > 0.0 ? total / weight : 0.0, 1.0);
}

// ---- Pass 5: split-sum BRDF LUT (scale, bias) --------------------------------
// Dispatch: (256/8, 256/8, 1). Output: R32G32Float.

RWTexture2D<float2> u_LUTOut : register(u0);

[numthreads(8, 8, 1)]
void BRDFLUTCS(uint3 id : SV_DispatchThreadID)
{
	float NdotV = max((id.x + 0.5) / u_FaceSize, 1e-4);
	float roughness = (id.y + 0.5) / u_FaceSize;

	float3 n = float3(0.0, 0.0, 1.0);
	float3 v = float3(sqrt(max(0.0, 1.0 - NdotV * NdotV)), 0.0, NdotV);

	float2 ab = 0.0;
	for (uint i = 0; i < u_SampleCount; ++i)
	{
		float2 xi = Hammersley(i, u_SampleCount);
		float3 h = ImportanceSampleGGX(xi, n, roughness);
		float3 l = normalize(2.0 * dot(v, h) * h - v);

		float NdotL = max(dot(n, l), 0.0);
		float NdotH = max(dot(n, h), 0.0);
		float VdotH = max(dot(v, h), 0.0);

		if (NdotL > 0.0)
		{
			float a2 = roughness * roughness * roughness * roughness;
			float ggxV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
			float ggxL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
			float G = 0.5 / max(ggxV + ggxL, 1e-6);
			float G_Vis = G * VdotH / max(NdotH * NdotV, 1e-5);

			float Fc = pow(1.0 - VdotH, 5.0);
			ab.x += (1.0 - Fc) * G_Vis;
			ab.y += Fc * G_Vis;
		}
	}

	u_LUTOut[id.xy] = ab / float(u_SampleCount);
}
