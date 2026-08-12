#pragma once

// =========================================================================
// IR — Intermediate Representation layer aggregate header
//
// The IR layer sits between the RHI abstract interface and concrete
// graphics backends (Vulkan, D3D12).  It provides shared infrastructure:
// resource tracking, PSO caching, descriptor set management, command
// validation, memory sub-allocation, and shader caching.
// =========================================================================

#include "Runtime/RHI/IR/IRTypes.h"
#include "Runtime/RHI/IR/IRResourceManager.h"
#include "Runtime/RHI/IR/IRPipelineCache.h"
#include "Runtime/RHI/IR/IRDescriptorSetManager.h"
#include "Runtime/RHI/IR/IRCommandValidator.h"
#include "Runtime/RHI/IR/IRMemoryAllocator.h"
#include "Runtime/RHI/IR/IRShaderLibrary.h"
#include "Runtime/RHI/IR/IRDevice.h"
