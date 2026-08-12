// DX12 Renderer2D Quad Shader — no texture for MVP phase
// Equivalent of Renderer2D_Quad.glsl adapted for HLSL SM 5.0

cbuffer TransformCB : register(b0)
{
	float4x4 u_ViewProjection;
};

struct VSInput
{
	float3 Position : POSITION;
	float4 Color    : COLOR;
	float2 TexCoord : TEXCOORD;
	float  TexIndex : TEXINDEX;
	float  TilingFactor : TILINGFACTOR;
	int    EntityID : ENTITYID;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 Color    : COLOR;
	int    EntityID : ENTITYID;
};

VSOutput VSMain(VSInput input)
{
	VSOutput output;
	output.Position = mul(u_ViewProjection, float4(input.Position, 1.0));
	output.Color    = input.Color;
	output.EntityID = input.EntityID;
	return output;
}

struct PSInput
{
	float4 Position : SV_POSITION;
	float4 Color    : COLOR;
	int    EntityID : ENTITYID;
};

struct PSOutput
{
	float4 Color    : SV_TARGET0;
	int    EntityID : SV_TARGET1;
};

PSOutput PSMain(PSInput input)
{
	PSOutput output;
	output.Color    = input.Color;
	output.EntityID = input.EntityID;
	return output;
}
