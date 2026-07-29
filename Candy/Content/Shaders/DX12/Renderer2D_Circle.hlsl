// DX12 Renderer2D Circle Shader — equivalent of Renderer2D_Circle.glsl

cbuffer TransformCB : register(b0)
{
	float4x4 u_ViewProjection;
};

struct VSInput
{
	float3 WorldPosition  : POSITION;
	float3 LocalPosition  : TEXCOORD0;
	float4 Color          : COLOR;
	float  Thickness      : TEXCOORD1;
	float  Fade           : TEXCOORD2;
	int    EntityID       : ENTITYID;
};

struct VSOutput
{
	float4 Position      : SV_POSITION;
	float3 LocalPosition : LOCALPOS;
	float4 Color         : COLOR;
	float  Thickness     : THICKNESS;
	float  Fade          : FADE;
	int    EntityID      : ENTITYID;
};

VSOutput VSMain(VSInput input)
{
	VSOutput output;
	output.Position      = mul(u_ViewProjection, float4(input.WorldPosition, 1.0));
	output.LocalPosition = input.LocalPosition;
	output.Color         = input.Color;
	output.Thickness     = input.Thickness;
	output.Fade          = input.Fade;
	output.EntityID      = input.EntityID;
	return output;
}

struct PSInput
{
	float4 Position      : SV_POSITION;
	float3 LocalPosition : LOCALPOS;
	float4 Color         : COLOR;
	float  Thickness     : THICKNESS;
	float  Fade          : FADE;
	int    EntityID      : ENTITYID;
};

struct PSOutput
{
	float4 Color    : SV_TARGET0;
	int    EntityID : SV_TARGET1;
};

PSOutput PSMain(PSInput input)
{
	float distance = 1.0 - length(input.LocalPosition);
	float circle   = smoothstep(0.0, input.Fade, distance);
	circle        *= smoothstep(input.Thickness + input.Fade, input.Thickness, distance);

	if (circle == 0.0)
		discard;

	PSOutput output;
	output.Color    = input.Color;
	output.Color.a *= circle;
	output.EntityID = input.EntityID;
	return output;
}
