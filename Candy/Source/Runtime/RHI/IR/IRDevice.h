#pragma once

#include "Runtime/RHI/RHIDevice.h"
#include "Runtime/RHI/IR/IRTypes.h"
#include "Runtime/RHI/IR/IRResourceManager.h"
#include "Runtime/RHI/IR/IRPipelineCache.h"
#include "Runtime/RHI/IR/IRDescriptorSetManager.h"
#include "Runtime/RHI/IR/IRCommandValidator.h"
#include "Runtime/RHI/IR/IRMemoryAllocator.h"
#include "Runtime/RHI/IR/IRShaderLibrary.h"

namespace Candy::IR {

	// =========================================================================
	// IRDevice — common base class for VulkanDevice and D3D12Device
	//
	// Aggregates all IR helper subsystems and provides shared logic that
	// both backends need: resource tracking, PSO caching, descriptor set
	// management, command validation, memory sub-allocation, and shader
	// caching.
	//
	// Concrete backends inherit from IRDevice and implement the RHIDevice
	// pure-virtual interface.
	// =========================================================================
	class IRDevice : public Candy::RHIDevice
	{
	public:
		IRDevice()          = default;
		virtual ~IRDevice() = default;

		// ---- IR Subsystem Access (non-owning references) -------------------

		[[nodiscard]] IRResourceManager&       GetResourceManager()       { return m_ResourceManager; }
		[[nodiscard]] const IRResourceManager& GetResourceManager() const { return m_ResourceManager; }

		[[nodiscard]] IRPipelineCache&         GetPipelineCache()         { return m_PipelineCache; }
		[[nodiscard]] const IRPipelineCache&   GetPipelineCache()   const { return m_PipelineCache; }

		[[nodiscard]] IRDescriptorSetManager&       GetDescriptorSetManager()       { return m_DescriptorSetManager; }
		[[nodiscard]] const IRDescriptorSetManager& GetDescriptorSetManager() const { return m_DescriptorSetManager; }

		[[nodiscard]] IRCommandValidator&      GetCommandValidator()       { return m_CommandValidator; }
		[[nodiscard]] const IRCommandValidator& GetCommandValidator() const { return m_CommandValidator; }

		[[nodiscard]] IRMemoryAllocator&       GetMemoryAllocator()       { return m_MemoryAllocator; }
		[[nodiscard]] const IRMemoryAllocator& GetMemoryAllocator() const { return m_MemoryAllocator; }

		[[nodiscard]] IRShaderLibrary&         GetShaderLibrary()         { return m_ShaderLibrary; }
		[[nodiscard]] const IRShaderLibrary&   GetShaderLibrary()   const { return m_ShaderLibrary; }

	protected:
		IRResourceManager      m_ResourceManager;
		IRPipelineCache        m_PipelineCache;
		IRDescriptorSetManager m_DescriptorSetManager;
		IRCommandValidator     m_CommandValidator;
		IRMemoryAllocator      m_MemoryAllocator;
		IRShaderLibrary        m_ShaderLibrary;
	};

} // namespace Candy::IR
