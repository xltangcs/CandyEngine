// CandyEngine built-in 2D sprite / circle shader (D3D12).
//
// "2D is just a mesh" (Paper2D): sprites/circles render through the unified
// SceneRenderer pipeline. This shader is the lightweight unlit counterpart of
// PBR.hlsl — no lighting, no TBN; a quad mesh with optional SDF circle
// clipping (u_CircleMode) and entity-ID output for picking.
//
// The cbuffer layout must match SceneRenderer's SpriteUniforms byte-for-byte
// (120B). The binding is a 256B-aligned slice, so reading less than 256B is
// fine.
//   CameraCB   (b0): u_ViewProjection, u_CameraPosition
//   MaterialCB (b1): u_World, u_BaseColor, 2D params, u_TextureFlags, u_EntityID
//   Textures (t0): base color map
//
// The //@param declarations drive material parameter reflection: the
// Builtin/Sprite and Builtin/Circle materials expose only these parameters
// in the inspector (no PBR noise).

cbuffer CameraCB : register(b0)
{
	float4x4 u_ViewProjection;
	float3    u_CameraPosition;
};

cbuffer MaterialCB : register(b1)
{
	float4x4 u_World;            // model -> world (column-major, matches GLM)
	float4 u_BaseColor;          // @param color "Color" = (1.0, 1.0, 1.0, 1.0)
	float  u_BlendMode;          // @param enum(Opaque, Masked, Transparent) "Blend Mode" = Transparent
	float  u_CircleMode;         // @param "Circle Mode" = 0.0   (0 = plain, 1 = SDF circle)
	float  u_Thickness;          // @param range(0.0, 1.0, 0.01) "Circle Thickness" = 1.0
	float  u_Fade;               // @param range(0.0, 0.1, 0.001) "Circle Fade" = 0.005
	uint   u_TextureFlags;       // bit0 base color
	int    u_EntityID;
	float2 u_UVTiling;           // @param "UV Tiling" = (1.0, 1.0)
	float2 u_UVOffset;           // @param "UV Offset" = (0.0, 0.0)
};

Texture2D    u_BaseColorMap;         // @param texture "Base Color Map"
SamplerState u_Sampler : register(s0);

struct VSInput
{
	float3 Position : TEXCOORD0;
	float3 Normal   : TEXCOORD1;   // unused — layout parity with MeshVertex
	float4 Tangent  : TEXCOORD2;   // unused — layout parity with MeshVertex
	float2 TexCoord : TEXCOORD3;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
	VSOutput output;
	output.Position = mul(u_ViewProjection, mul(u_World, float4(input.Position, 1.0)));
	output.TexCoord = input.TexCoord * u_UVTiling + u_UVOffset;
	return output;
}

struct PSOutput
{
	float4 Color    : SV_TARGET0;
	int    EntityID : SV_TARGET1;
};

PSOutput PSMain(VSOutput input)
{
	float4 baseColor = u_BaseColor;
	if (u_TextureFlags & 1)
		baseColor *= u_BaseColorMap.Sample(u_Sampler, input.TexCoord);

	if (u_BlendMode >= 0.5 && u_BlendMode < 1.5) // Masked
		clip(baseColor.a - 0.5);

	// --- SDF circle (u_CircleMode = 1) -------------------------------------
	// No dedicated circle geometry: the quad's UV doubles as quad-local
	// position (TexCoord*2-1 ∈ [-1,1]); the pixel shader carves the disc.
	// Same distance field as the former dedicated circle shader — visually
	// zero-diff, including the ring case (u_Thickness < 1).
	if (u_CircleMode >= 0.5)
	{
		float2 localPos = input.TexCoord * 2.0 - 1.0;
		float  distance = 1.0 - length(localPos);
		float  circle   = smoothstep(0.0, u_Fade, distance);
		circle         *= smoothstep(u_Thickness + u_Fade, u_Thickness, distance);
		if (circle == 0.0)
			discard;
		baseColor.a *= circle;
	}

	PSOutput output;
	output.Color    = baseColor;
	output.EntityID = u_EntityID;
	return output;
}
