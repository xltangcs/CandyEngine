#pragma once

// =========================================================================
// RHI/Shared — backend-shared infrastructure aggregate header
//
// This is NOT a shader IR (CandyEngine does not use SPIR-V as a
// cross-backend intermediate representation — see AGENTS.md). It is the
// shared implementation layer between the RHI abstract interface and the
// concrete backends (D3D12, OpenGL, Vulkan): resource tracking, PSO
// caching, descriptor set management, command validation, memory
// sub-allocation, and shader caching. Backends get all of it for free by
// inheriting RHIDeviceBase (same role as Godot's RenderingDeviceCommons).
// =========================================================================

#include "Runtime/RHI/Shared/RHISharedTypes.h"
#include "Runtime/RHI/Shared/RHIResourceManager.h"
#include "Runtime/RHI/Shared/RHIPipelineCache.h"
#include "Runtime/RHI/Shared/RHIDescriptorSetManager.h"
#include "Runtime/RHI/Shared/RHICommandValidator.h"
#include "Runtime/RHI/Shared/RHIMemoryAllocator.h"
#include "Runtime/RHI/Shared/RHIShaderLibrary.h"
#include "Runtime/RHI/Shared/RHIDeviceBase.h"
