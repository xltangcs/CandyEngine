// CandyEngine example shader --- demonstrates the //@param reflection
// convention. Not part of the render pipeline; used by the material
// inspector to build the parameter UI (Godot-style uniform reflection).
//
// Syntax:
//   // @param <hint> "Display Name" = <default>
//   hint: color | range(min, max, step) | enum(A, B, C) | texture | (none)

cbuffer MaterialCB : register(b1)
{
	float4 u_BaseColor;          // @param color "Base Color" = (1.0, 1.0, 1.0, 1.0)
	float  u_Metallic;           // @param range(0.0, 1.0, 0.01) "Metallic" = 0.0
	float  u_Roughness;          // @param range(0.0, 1.0, 0.01) "Roughness" = 0.5
	float  u_BlendMode;          // @param enum(Opaque, Masked, Transparent) "Blend Mode" = Opaque
	float3 u_Emissive;           // @param "Emissive Color" = (0.0, 0.0, 0.0)
	float  u_EmissiveIntensity;  // @param range(0.0, 10.0, 0.1) "Emissive Intensity" = 0.0
	int    u_MaxLights;          // @param range(0, 8, 1) "Max Lights" = 4
	bool   u_UseAOMap;           // @param "Use AO Map" = false
};

Texture2D    u_AlbedoMap : register(t0);  // @param texture "Albedo Map"
Texture2D    u_NormalMap : register(t1);  // @param texture "Normal Map"
SamplerState u_Sampler   : register(s0);
