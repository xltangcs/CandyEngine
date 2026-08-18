// CandyEngine debug line overlay shader (D3D12).
//
// Used for editor collider wireframes etc.: pass-through vertices with
// view-projection, solid per-vertex color + entity ID (picking).
//
// Binds only CameraCB (b0) — the shared textured root signature may declare
// more parameters (b1 CBV + SRV table) than this shader consumes.
//
// Vertex inputs use TEXCOORD0..N semantics to match the RHI vertex layout
// (location == semantic index), see D3D12Device::CreateGraphicsPipeline.

cbuffer CameraCB : register(b0)
{
	float4x4 u_ViewProjection;
	float3    u_CameraPosition;
};

struct VSInput
{
	float3 Position : TEXCOORD0;
	float4 Color    : TEXCOORD1;
	int    EntityID : TEXCOORD2;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 Color    : TEXCOORD0;
	int    EntityID : TEXCOORD1;
};

VSOutput VSMain(VSInput input)
{
	VSOutput output;
	output.Position = mul(u_ViewProjection, float4(input.Position, 1.0));
	output.Color    = input.Color;
	output.EntityID = input.EntityID;
	return output;
}

struct PSOutput
{
	float4 Color    : SV_TARGET0;
	int    EntityID : SV_TARGET1;
};

PSOutput PSMain(VSOutput input)
{
	PSOutput output;
	output.Color    = input.Color;
	output.EntityID = input.EntityID;
	return output;
}
