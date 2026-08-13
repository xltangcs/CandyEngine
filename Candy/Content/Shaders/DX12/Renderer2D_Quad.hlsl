// DX12 Renderer2D Quad Shader — textured batch
// Input layout uses TEXCOORD0..5 semantics to match the engine's PSO input
// layout (semantic name "TEXCOORD" + semantic index = attribute location).

cbuffer TransformCB : register(b0)
{
	float4x4 u_ViewProjection;
};

Texture2D    u_Textures[32] : register(t0);
SamplerState u_Sampler      : register(s0);

struct VSInput
{
	float3 Position    : TEXCOORD0;
	float4 Color       : TEXCOORD1;
	float2 TexCoord    : TEXCOORD2;
	float  TexIndex    : TEXCOORD3;
	float  TilingFactor : TEXCOORD4;
	int    EntityID    : TEXCOORD5;
};

struct VSOutput
{
	float4 Position    : SV_POSITION;
	float4 Color       : COLOR;
	float2 TexCoord    : TEXCOORD;
	float  TexIndex    : TEXINDEX;
	float  TilingFactor : TILINGFACTOR;
	int    EntityID    : ENTITYID;
};

VSOutput VSMain(VSInput input)
{
	VSOutput output;
	output.Position     = mul(u_ViewProjection, float4(input.Position, 1.0));
	output.Color        = input.Color;
	output.TexCoord     = input.TexCoord;
	output.TexIndex     = input.TexIndex;
	output.TilingFactor = input.TilingFactor;
	output.EntityID     = input.EntityID;
	return output;
}

struct PSInput
{
	float4 Position    : SV_POSITION;
	float4 Color       : COLOR;
	float2 TexCoord    : TEXCOORD;
	float  TexIndex    : TEXINDEX;
	float  TilingFactor : TILINGFACTOR;
	int    EntityID    : ENTITYID;
};

struct PSOutput
{
	float4 Color    : SV_TARGET0;
	int    EntityID : SV_TARGET1;
};

PSOutput PSMain(PSInput input)
{
	float2 uv = input.TexCoord * input.TilingFactor;
	int idx = max(0, min(31, int(input.TexIndex)));
	float4 texColor = input.Color;
	// SM5.0 forbids dynamic indexing into Texture2D[32]; unroll into 32
	// literal-indexed Sample calls so the bindless-ish pattern is valid.
	[unroll] for (int t = 0; t < 32; ++t)
	{
		[unroll] if (t == idx)
			texColor = input.Color * u_Textures[t].Sample(u_Sampler, uv);
	}

	PSOutput output;
	output.Color    = texColor;
	output.EntityID = input.EntityID;
	return output;
}
