#pragma once

#include <string>
#include <unordered_map>

#include "Runtime/RHI/RHITypes.h"

namespace Candy {

	class RHIShaderSource
	{
	public:
		/// Parses a source file with `#type vertex` / `#type fragment` markers into
		/// a map of ShaderStage -> source string.
		static std::unordered_map<ShaderStage, std::string> Parse(const std::string& source);

		/// Converts a ShaderStage enum to its `#type` tag string.
		static const char* StageToString(ShaderStage stage);
	};

} // namespace Candy
