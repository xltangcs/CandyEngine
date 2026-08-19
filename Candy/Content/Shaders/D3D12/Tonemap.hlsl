// CandyEngine tonemap pass (D3D12).
//
// Fullscreen triangle (SV_VertexID, no vertex input) that converts the HDR
// scene color attachment (R16G16B16A16Float, linear) to LDR (RGBA8, sRGB)
// for display. Runs after the scene pass; the game UI composites after this
// pass so UI colors are not tonemapped.
//
// Layout conventions:
//   TonemapCB (b0): u_Exposure (global scene exposure; 1.0 = neutral)
//   Texture  (t0): HDR scene color attachment

cbuffer TonemapCB : register(b0)
{
	float u_Exposure;
	float3 _Pad;
};

Texture2D   u_SceneColor : register(t0);
SamplerState u_Sampler : register(s0);

struct VSOutput
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

VSOutput VSMain(uint vertexID : SV_VertexID)
{
	VSOutput output;
	// Fullscreen triangle: (0,0), (2,0), (0,2) in UV-space.
	output.TexCoord = float2((vertexID << 1) & 2, vertexID & 2);
	output.Position = float4(output.TexCoord * 2.0 - 1.0, 0.0, 1.0);
	return output;
}

// ACES filmic tonemap (Narkowicz 2015 approximation).
float3 ACESFilm(float3 x)
{
	return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}

// Linear -> sRGB (exact piecewise curve).
float3 LinearToSRGB(float3 c)
{
	float3 lo = c * 12.92;
	float3 hi = 1.055 * pow(max(c, 0.0), 1.0 / 2.4) - 0.055;
	return (c <= 0.0031308) ? lo : hi;
}

struct PSOutput
{
	float4 Color : SV_TARGET0;
};

PSOutput PSMain(VSOutput input)
{
	float3 hdr = u_SceneColor.Sample(u_Sampler, input.TexCoord).rgb;

	hdr *= u_Exposure;
	hdr  = ACESFilm(hdr);
	hdr  = LinearToSRGB(hdr);

	PSOutput output;
	output.Color = float4(hdr, 1.0);
	return output;
}
