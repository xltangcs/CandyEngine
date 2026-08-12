#pragma once

#include <cstdint>
#include <vector>

namespace Candy::VulkanSPIRV {

	// =========================================================================
	// Minimal triangle vertex shader SPIR-V
	//
	// GLSL equivalent:
	//   layout(location=0) in vec3 aPos;
	//   layout(location=1) in vec4 aColor;
	//   layout(location=0) out vec4 vColor;
	//   void main() { gl_Position = vec4(aPos,1.0); vColor = aColor; }
	// =========================================================================
	inline const std::vector<uint32_t>& GetTriangleVS()
	{
		// SPIR-V 1.0, hand-crafted, bound=19
		static const std::vector<uint32_t> vs = {
			// Header
			0x07230203, 0x00010000, 0x00000000, 0x00000013, 0x00000000,
			// OpCapability Shader
			0x00020011, 0x00000001,
			// OpExtInstImport %1 "GLSL.std.450"
			0x0006000b, 0x00000001, 0x4c534c47, 0x2e647473, 0x2e303534, 0x00000000,
			// OpMemoryModel Logical GLSL450
			0x0003000e, 0x00000000, 0x00000001,
			// OpEntryPoint Vertex %main "main" %inPos %inColor %gl_Pos %outColor
			0x0008000f, 0x00000000, 0x0000000e, 0x6e69616d, 0x00000000,
			0x0000000a, 0x0000000b, 0x0000000c, 0x0000000d,
			// OpSource GLSL 450
			0x00030003, 0x00000002, 0x000001c2,
			// OpName %main "main"
			0x00040005, 0x0000000e, 0x6e69616d, 0x00000000,
			// OpDecorate %inPos Location 0
			0x00040047, 0x0000000a, 0x0000001e, 0x00000000,
			// OpDecorate %inColor Location 1
			0x00040047, 0x0000000b, 0x0000001e, 0x00000001,
			// OpDecorate %gl_Pos BuiltIn Position
			0x00040047, 0x0000000c, 0x0000000b, 0x00000000,
			// OpDecorate %outColor Location 0
			0x00040047, 0x0000000d, 0x0000001e, 0x00000000,
			// %void = OpTypeVoid
			0x00020013, 0x00000002,
			// %func = OpTypeFunction %void
			0x00030015, 0x00000003, 0x00000002,
			// %float = OpTypeFloat 32
			0x00030016, 0x00000004, 0x00000020,
			// %v3float = OpTypeVector %float 3
			0x00040017, 0x00000005, 0x00000004, 0x00000003,
			// %v4float = OpTypeVector %float 4
			0x00040017, 0x00000006, 0x00000004, 0x00000004,
			// %ptr_in_v3 = OpTypePointer Input %v3float
			0x00040020, 0x00000007, 0x00000001, 0x00000005,
			// %ptr_in_v4 = OpTypePointer Input %v4float
			0x00040020, 0x00000008, 0x00000001, 0x00000006,
			// %ptr_out_v4 = OpTypePointer Output %v4float
			0x00040020, 0x00000009, 0x00000003, 0x00000006,
			// %inPos = OpVariable %ptr_in_v3 Input
			0x0004003b, 0x00000007, 0x0000000a, 0x00000001,
			// %inColor = OpVariable %ptr_in_v4 Input
			0x0004003b, 0x00000008, 0x0000000b, 0x00000001,
			// %gl_Pos = OpVariable %ptr_out_v4 Output
			0x0004003b, 0x00000009, 0x0000000c, 0x00000003,
			// %outColor = OpVariable %ptr_out_v4 Output
			0x0004003b, 0x00000009, 0x0000000d, 0x00000003,
			// %float_1 = OpConstant %float 1.0
			0x0007002b, 0x00000004, 0x0000000f, 0x3f800000,
			// %main = OpFunction %void None %func
			0x00050036, 0x00000002, 0x0000000e, 0x00000000, 0x00000003,
			// OpLabel (%16)
			0x000200f8, 0x00000010,
			// %posVal = OpLoad %v3float %inPos
			0x0004003e, 0x00000005, 0x00000011, 0x0000000a,
			// %colVal = OpLoad %v4float %inColor
			0x0004003e, 0x00000006, 0x00000012, 0x0000000b,
			// %pos4 = OpCompositeConstruct %v4float %posVal %float_1
			0x00070034, 0x00000006, 0x00000013, 0x00000011, 0x0000000f,
			// OpStore %gl_Pos %pos4
			0x0003003d, 0x0000000c, 0x00000013,
			// OpStore %outColor %colVal
			0x0003003d, 0x0000000d, 0x00000012,
			// OpReturn
			0x000100fd,
			// OpFunctionEnd
			0x00010038,
		};
		return vs;
	}

	// =========================================================================
	// Minimal triangle fragment shader SPIR-V
	//
	// GLSL equivalent:
	//   layout(location=0) in vec4 vColor;
	//   layout(location=0) out vec4 fragColor;
	//   void main() { fragColor = vColor; }
	// =========================================================================
	inline const std::vector<uint32_t>& GetTrianglePS()
	{
		// SPIR-V 1.0, hand-crafted, bound=14
		static const std::vector<uint32_t> ps = {
			// Header
			0x07230203, 0x00010000, 0x00000000, 0x0000000e, 0x00000000,
			// OpCapability Shader
			0x00020011, 0x00000001,
			// OpExtInstImport %1 "GLSL.std.450"
			0x0006000b, 0x00000001, 0x4c534c47, 0x2e647473, 0x2e303534, 0x00000000,
			// OpMemoryModel Logical GLSL450
			0x0003000e, 0x00000000, 0x00000001,
			// OpEntryPoint Fragment %main "main" %inColor %outColor
			0x0006000f, 0x00000004, 0x00000009, 0x6e69616d, 0x00000000,
			0x00000007, 0x00000008,
			// OpExecutionMode %main OriginUpperLeft
			0x00030010, 0x00000009, 0x00000000,
			// OpSource GLSL 450
			0x00030003, 0x00000002, 0x000001c2,
			// OpName %main "main"
			0x00040005, 0x00000009, 0x6e69616d, 0x00000000,
			// OpDecorate %inColor Location 0
			0x00040047, 0x00000007, 0x0000001e, 0x00000000,
			// OpDecorate %outColor Location 0
			0x00040047, 0x00000008, 0x0000001e, 0x00000000,
			// %void = OpTypeVoid
			0x00020013, 0x00000002,
			// %func = OpTypeFunction %void
			0x00030015, 0x00000003, 0x00000002,
			// %float = OpTypeFloat 32
			0x00030016, 0x00000004, 0x00000020,
			// %v4float = OpTypeVector %float 4
			0x00040017, 0x00000005, 0x00000004, 0x00000004,
			// %ptr_in = OpTypePointer Input %v4float
			0x00040020, 0x00000006, 0x00000001, 0x00000005,
			// %ptr_out = OpTypePointer Output %v4float
			0x00040020, 0x0000000a, 0x00000003, 0x00000005,
			// %inColor = OpVariable %ptr_in Input
			0x0004003b, 0x00000006, 0x00000007, 0x00000001,
			// %outColor = OpVariable %ptr_out Output
			0x0004003b, 0x0000000a, 0x00000008, 0x00000003,
			// %main = OpFunction %void None %func
			0x00050036, 0x00000002, 0x00000009, 0x00000000, 0x00000003,
			// OpLabel (%b)
			0x000200f8, 0x0000000b,
			// %val = OpLoad %v4float %inColor
			0x0004003e, 0x00000005, 0x0000000c, 0x00000007,
			// OpStore %outColor %val
			0x0003003d, 0x00000008, 0x0000000c,
			// OpReturn
			0x000100fd,
			// OpFunctionEnd
			0x00010038,
		};
		return ps;
	}

} // namespace Candy::VulkanSPIRV
