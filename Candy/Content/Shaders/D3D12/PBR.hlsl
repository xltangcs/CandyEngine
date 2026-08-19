// CandyEngine built-in PBR surface shader (D3D12).
//
// The //@param declarations drive material parameter reflection in the
// inspector (Godot-style): MeshImporter fills the conventional keys
// (u_BaseColor, ...) when importing glTF materials. SceneRenderer packs the
// reflected values into MaterialCB (b1) at runtime.
//
// Layout conventions (SceneRenderer MaterialUniforms matches byte-for-byte):
//   CameraCB   (b0): u_ViewProjection, u_CameraPosition
//   MaterialCB (b1): u_World, surface params, u_TextureFlags, u_EntityID
//   LightCB    (b2): u_Lights[16], u_AmbientColor, u_NumLights, IBL params
//   Textures (t0-t3): base color / metallic-roughness / normal / emissive
//   Textures (t4-t6): IBL — irradiance cube / prefiltered cube / BRDF LUT
//                     (bound once per pass by SceneRenderer, not @param)

cbuffer CameraCB : register(b0)
{
	float4x4 u_ViewProjection;
	float3    u_CameraPosition;
};

cbuffer MaterialCB : register(b1)
{
	float4x4 u_World;            // model -> world (column-major, matches GLM)
	float4 u_BaseColor;          // @param color "Base Color" = (1.0, 1.0, 1.0, 1.0)
	float  u_Metallic;           // @param range(0.0, 1.0, 0.01) "Metallic" = 0.0
	float  u_Roughness;          // @param range(0.0, 1.0, 0.01) "Roughness" = 0.5
	float  u_BlendMode;          // @param enum(Opaque, Masked, Transparent) "Blend Mode" = Opaque
	float  u_ShadingModel;       // @param enum(Lit, Unlit) "Shading Model" = Lit
	float3 u_Emissive;           // @param "Emissive Color" = (0.0, 0.0, 0.0)
	float  u_EmissiveIntensity;  // @param range(0.0, 10.0, 0.1) "Emissive Intensity" = 0.0
	float2 u_UVTiling;           // @param "UV Tiling" = (1.0, 1.0)
	float2 u_UVOffset;           // @param "UV Offset" = (0.0, 0.0)
	uint   u_TextureFlags;       // bit0 base color, bit1 metallic-roughness, bit2 normal, bit3 emissive
	int    u_EntityID;
};

struct LightData
{
	float3 Direction;            // world-space travel direction (Directional/Spot)
	float  Type;                 // 0 Directional, 1 Point, 2 Spot
	float3 Position;             // world position (Point/Spot)
	float  Range;                // Point/Spot range
	float3 Color;
	float  Intensity;
	float2 ConeCos;              // spot (inner, outer) cosines, precomputed CPU-side
	float2 _Pad;
};

cbuffer LightCB : register(b2)
{
	LightData u_Lights[16];
	float3    u_AmbientColor;
	int       u_NumLights;
	float     u_IBLIntensity;      // multiplier on the IBL contribution
	float     u_MaxReflectionLod;  // prefiltered map mips - 1
	int       u_IBLEnabled;        // 0 = legacy ambient, 1 = split-sum IBL
	float     _IBLPad;
};

Texture2D    u_BaseColorMap;         // @param texture "Base Color Map" --- color map (sRGB-decoded per engine convention)
Texture2D    u_MetallicRoughnessMap; // @param texture "Metallic-Roughness Map" (G = roughness, B = metallic)
Texture2D    u_NormalMap;            // @param texture "Normal Map"
Texture2D    u_EmissiveMap;          // @param texture "Emissive Map" --- color map (sRGB-decoded per engine convention)
SamplerState u_Sampler : register(s0);

// IBL environment maps (bound once per pass by SceneRenderer).
TextureCube u_IrradianceMap   : register(t4);
TextureCube u_PrefilteredMap  : register(t5);
Texture2D   u_BRDFLUT         : register(t6);

// =============================================================================
// PBR lighting — simplified Cook-Torrance, forward light loop (max 16 lights)
// =============================================================================

static const float kMaskCutoff = 0.5;

// Accumulates radiance + attenuation for one light; returns (L, radiance).
float3 EvaluateLight(int index, float3 worldPos, out float3 L)
{
	LightData light = u_Lights[index];

	float3 radiance;
	float  attenuation = 1.0;

	if (light.Type < 0.5) // Directional: fixed direction, no attenuation
	{
		L = -normalize(light.Direction);
	}
	else if (light.Type < 1.5) // Point: inverse-distance with range falloff
	{
		float3 toLight = light.Position - worldPos;
		float  dist    = length(toLight);
		L = toLight / max(dist, 1e-4);
		float rangeFade = saturate(1.0 - dist / max(light.Range, 0.001));
		attenuation = rangeFade * rangeFade;
	}
	else // Spot: point falloff * smooth cone (inner/outer cosines)
	{
		float3 toLight = light.Position - worldPos;
		float  dist    = length(toLight);
		L = toLight / max(dist, 1e-4);
		float rangeFade = saturate(1.0 - dist / max(light.Range, 0.001));
		float cosTheta  = saturate(dot(-L, normalize(light.Direction)));
		float coneFade  = saturate((cosTheta - light.ConeCos.y) / max(light.ConeCos.x - light.ConeCos.y, 1e-5));
		attenuation = rangeFade * rangeFade * coneFade * coneFade;
	}

	radiance = light.Color * light.Intensity * attenuation;
	return radiance;
}

float3 F_Schlick(float3 f0, float VdotH)
{
	return f0 + (1.0 - f0) * pow(1.0 - VdotH, 5.0);
}

float D_GGX(float NdotH, float roughness)
{
	float a  = roughness * roughness;
	float a2 = a * a;
	float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
	return a2 / max(3.14159265 * d * d, 1e-6);
}

float V_SmithGGXCorrelated(float NdotV, float NdotL, float roughness)
{
	float a2 = roughness * roughness * roughness * roughness;
	float ggxV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
	float ggxL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
	return 0.5 / max(ggxV + ggxL, 1e-6);
}

// =============================================================================
// Vertex shader
// =============================================================================

struct VSInput
{
	float3 Position : TEXCOORD0;
	float3 Normal   : TEXCOORD1;
	float4 Tangent  : TEXCOORD2;
	float2 TexCoord : TEXCOORD3;
};

struct VSOutput
{
	float4 Position        : SV_POSITION;
	float3 WorldPosition   : TEXCOORD0;
	float3 WorldNormal     : TEXCOORD1;
	float3 WorldTangent    : TEXCOORD2;
	float3 WorldBitangent  : TEXCOORD3;
	float2 TexCoord        : TEXCOORD4;
};

VSOutput VSMain(VSInput input)
{
	VSOutput output;
	float4 worldPos = mul(u_World, float4(input.Position, 1.0));
	output.Position       = mul(u_ViewProjection, worldPos);
	output.WorldPosition  = worldPos.xyz;
	output.WorldNormal    = normalize(mul((float3x3)u_World, input.Normal));
	output.WorldTangent   = normalize(mul((float3x3)u_World, input.Tangent.xyz));
	output.WorldBitangent = cross(output.WorldNormal, output.WorldTangent) * input.Tangent.w;
	output.TexCoord       = input.TexCoord * u_UVTiling + u_UVOffset;
	return output;
}

// =============================================================================
// Pixel shader
// =============================================================================

struct PSOutput
{
	float4 Color    : SV_TARGET0;
	int    EntityID : SV_TARGET1;
};

PSOutput PSMain(VSOutput input)
{
	// --- Surface data -----------------------------------------------------
	float4 baseColor = u_BaseColor;
	if (u_TextureFlags & 1)
		baseColor *= u_BaseColorMap.Sample(u_Sampler, input.TexCoord);

	if (u_BlendMode >= 0.5 && u_BlendMode < 1.5) // Masked
		clip(baseColor.a - kMaskCutoff);

	float metallic  = u_Metallic;
	float roughness = u_Roughness;
	if (u_TextureFlags & 2)
	{
		float4 mr = u_MetallicRoughnessMap.Sample(u_Sampler, input.TexCoord);
		roughness *= mr.g; // glTF: G = roughness
		metallic  *= mr.b; // glTF: B = metallic
	}

	float3 N = normalize(input.WorldNormal);
	if (u_TextureFlags & 4)
	{
		float3 tn = u_NormalMap.Sample(u_Sampler, input.TexCoord).xyz * 2.0 - 1.0;
		N = normalize(tn.x * input.WorldTangent + tn.y * input.WorldBitangent + tn.z * N);
	}

	// --- Unlit ------------------------------------------------------------
	float3 color;
	if (u_ShadingModel >= 0.5) // Unlit
	{
		color = baseColor.rgb;
	}
	else
	{
		// --- Lighting (simplified Cook-Torrance, forward light loop) --------
		float3 V = normalize(u_CameraPosition - input.WorldPosition);
		float NdotV = max(dot(N, V), 1e-4);

		float3 f0      = lerp(float3(0.04, 0.04, 0.04), baseColor.rgb, metallic);
		float3 diffuse = (1.0 - f0) * (1.0 - metallic) * baseColor.rgb;

			float3 direct = 0.0;
			for (int i = 0; i < u_NumLights; ++i)
			{
				float3 L;
				float3 radiance = EvaluateLight(i, input.WorldPosition, L);

				float NdotL = saturate(dot(N, L));
				float3 H = normalize(V + L);
				float NdotH = saturate(dot(N, H));
				float VdotH = saturate(dot(V, H));

				float D   = D_GGX(NdotH, roughness);
				float Vis = V_SmithGGXCorrelated(NdotV, NdotL, roughness);
				float3 spec = F_Schlick(f0, VdotH) * D * Vis;

				direct += (diffuse + spec) * radiance * NdotL;
			}

			// --- Image-based lighting (split-sum, CPU-baked environment) ----
			float3 ambient;
			if (u_IBLEnabled > 0)
			{
				float3 irradiance = u_IrradianceMap.Sample(u_Sampler, N).rgb;

				float3 R = reflect(-V, N);
				float3 prefiltered = u_PrefilteredMap.SampleLevel(
					u_Sampler, R, roughness * u_MaxReflectionLod).rgb;

				float2 brdf = u_BRDFLUT.Sample(u_Sampler, float2(NdotV, roughness)).rg;

				ambient = diffuse * irradiance + prefiltered * (f0 * brdf.x + brdf.y);
				ambient *= u_IBLIntensity;
			}
			else
			{
				ambient = u_AmbientColor * baseColor.rgb;
			}

			color = direct + ambient;
	}

	// --- Emissive -----------------------------------------------------------
	float3 emissive = u_Emissive * u_EmissiveIntensity;
	if (u_TextureFlags & 8)
		emissive *= u_EmissiveMap.Sample(u_Sampler, input.TexCoord).rgb;
	color += emissive;

	PSOutput output;
	output.Color    = float4(color, baseColor.a);
	output.EntityID = u_EntityID;
	return output;
}
