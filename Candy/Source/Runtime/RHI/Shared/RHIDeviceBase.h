#pragma once

#include "Runtime/RHI/RHIDevice.h"
#include "Runtime/RHI/Shared/RHISharedTypes.h"
#include "Runtime/RHI/Shared/RHIResourceManager.h"
#include "Runtime/RHI/Shared/RHIPipelineCache.h"
#include "Runtime/RHI/Shared/RHIDescriptorSetManager.h"
#include "Runtime/RHI/Shared/RHICommandValidator.h"
#include "Runtime/RHI/Shared/RHIMemoryAllocator.h"
#include "Runtime/RHI/Shared/RHIShaderLibrary.h"

namespace Candy {

	// =========================================================================
	// RHIDeviceBase — common base class for the backend devices
	// (D3D12Device, OpenGLRHIDevice, VulkanDevice).
	//
	// Aggregates all shared RHI infrastructure subsystems and provides the
	// logic every backend needs: resource tracking, PSO caching, descriptor
	// set management, command validation, memory sub-allocation, and shader
	// caching. Same role as Godot's RenderingDeviceCommons.
	//
	// Concrete backends inherit from RHIDeviceBase and implement the
	// RHIDevice pure-virtual interface.
	// =========================================================================
	class RHIDeviceBase : public RHIDevice
	{
	public:
		RHIDeviceBase()          = default;
		virtual ~RHIDeviceBase() = default;

		// ---- Shared Subsystem Access (non-owning references) ----------------

		[[nodiscard]] RHIResourceManager&       GetResourceManager()       { return m_ResourceManager; }
		[[nodiscard]] const RHIResourceManager& GetResourceManager() const { return m_ResourceManager; }

		[[nodiscard]] RHIPipelineCache&         GetPipelineCache()         { return m_PipelineCache; }
		[[nodiscard]] const RHIPipelineCache&   GetPipelineCache()   const { return m_PipelineCache; }

		[[nodiscard]] RHIDescriptorSetManager&       GetDescriptorSetManager()       { return m_DescriptorSetManager; }
		[[nodiscard]] const RHIDescriptorSetManager& GetDescriptorSetManager() const { return m_DescriptorSetManager; }

		[[nodiscard]] RHICommandValidator&      GetCommandValidator()       { return m_CommandValidator; }
		[[nodiscard]] const RHICommandValidator& GetCommandValidator() const { return m_CommandValidator; }

		[[nodiscard]] RHIMemoryAllocator&       GetMemoryAllocator()       { return m_MemoryAllocator; }
		[[nodiscard]] const RHIMemoryAllocator& GetMemoryAllocator() const { return m_MemoryAllocator; }

		[[nodiscard]] RHIShaderLibrary&         GetShaderLibrary()         { return m_ShaderLibrary; }
		[[nodiscard]] const RHIShaderLibrary&   GetShaderLibrary()   const { return m_ShaderLibrary; }

	protected:
		RHIResourceManager      m_ResourceManager;
		RHIPipelineCache        m_PipelineCache;
		RHIDescriptorSetManager m_DescriptorSetManager;
		RHICommandValidator     m_CommandValidator;
		RHIMemoryAllocator      m_MemoryAllocator;
		RHIShaderLibrary        m_ShaderLibrary;
	};

} // namespace Candy
