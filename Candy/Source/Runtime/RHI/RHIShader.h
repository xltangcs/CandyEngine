#pragma once

#include "Runtime/Core/Base.h"
#include "Runtime/RHI/RHITypes.h"

#include <cstdint>
#include <string>

namespace Candy {

	// =========================================================================
	// RHIShaderModule — a compiled SPIR-V shader module for a single stage
	// =========================================================================
	class RHIShaderModule
	{
	public:
		virtual ~RHIShaderModule() = default;

		virtual ShaderStage GetStage() const = 0;

		/// Returns a read-only view of the SPIR-V bytecode.
		virtual const uint32_t* GetBytecode() const = 0;
		virtual uint32_t        GetBytecodeSize() const = 0; // in bytes

		virtual const std::string& GetDebugName() const = 0;
	};

} // namespace Candy
