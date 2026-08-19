// CandyEngine skybox shader (D3D12).
//
// Fullscreen triangle (SV_VertexID, no vertex input) drawn first inside the
// scene render pass with depth LessEqual (opaque geometry writes z < 1.0 and
// occludes it). The view direction is reconstructed from the inverse
// view-projection matrix (CameraCB b0, see SceneRenderer::CameraUniforms),
// so the skybox is always centered on the camera regardless of position.
//
// Layout conventions:
//   CameraCB (b0): u_ViewProjection, u_CameraPosition, u_InvViewProjection
//   SkyboxCB (b1): u_Exposure, u_Intensity (16B, matches SkyboxUniforms)
//   Texture  (t0): environment cubemap (TextureCube)

cbuffer CameraCB : register(b0)
{
	float4x4 u_ViewProjection;
	float3    u_CameraPosition;
	float     _CamPad;
	float4x4 u_InvViewProjection;
};

cbuffer SkyboxCB : register(b1)
{
	float u_Exposure;
	float u_Intensity;
	float2 _Pad;
};

TextureCube   u_SkyboxMap : register(t0);
SamplerState  u_Sampler : register(s0);

struct VSOutput
{
	float4 Position : SV_POSITION;
	float2 NDC      : TEXCOORD0;
};

VSOutput VSMain(uint vertexID : SV_VertexID)
{
	VSOutput output;
	// Fullscreen triangle: (0,0), (2,0), (0,2) in NDC-space.
	output.NDC      = float2((vertexID << 1) & 2, vertexID & 2);
	output.Position = float4(output.NDC * 2.0 - 1.0, 1.0, 1.0); // z = far plane
	return output;
}

struct PSOutput
{
	float4 Color    : SV_TARGET0;
	int    EntityID : SV_TARGET1;
};

PSOutput PSMain(VSOutput input)
{
	// Unproject the far-plane point back into world space. input.NDC
	// interpolates the raw (0,0),(2,0),(0,2) vertex values -> [0,1]^2, so it
	// must be mapped to clip space [-1,1]^2 before unprojection.
	float4 clipPos = float4(input.NDC * 2.0 - 1.0, 1.0, 1.0);
	float4 world   = mul(u_InvViewProjection, clipPos);
	float3 dir     = normalize(world.xyz / world.w);

	float3 color = u_SkyboxMap.Sample(u_Sampler, dir).rgb;

	// Linear HDR output: the scene pass renders to a float16 target, and the
	// tonemap pass (Tonemap.hlsl) applies exposure + ACES + sRGB encoding.
	color *= u_Exposure;
	color *= u_Intensity;

	PSOutput output;
	output.Color    = float4(color, 1.0);
	output.EntityID = -1; // clicking the sky deselects
	return output;
}
