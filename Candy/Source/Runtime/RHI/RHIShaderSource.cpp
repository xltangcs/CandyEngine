#include "CandyPCH.h"

#include "Runtime/RHI/RHIShaderSource.h"

namespace Candy {

	static ShaderStage ShaderStageFromString(const std::string& type)
	{
		if (type == "vertex")
			return ShaderStage::Vertex;
		if (type == "fragment" || type == "pixel")
			return ShaderStage::Fragment;

		CANDY_CORE_ASSERT(false, "Unknown shader type!");
		return ShaderStage::None;
	}

	const char* RHIShaderSource::StageToString(ShaderStage stage)
	{
		switch (stage)
		{
		case ShaderStage::Vertex:    return "vertex";
		case ShaderStage::Fragment:  return "fragment";
		case ShaderStage::Geometry:  return "geometry";
		case ShaderStage::Compute:   return "compute";
		default:                     return "unknown";
		}
	}

	std::unordered_map<ShaderStage, std::string> RHIShaderSource::Parse(const std::string& source)
	{
		std::unordered_map<ShaderStage, std::string> shaderSources;

		const char* typeToken = "#type";
		size_t typeTokenLength = strlen(typeToken);
		size_t pos = source.find(typeToken, 0);
		while (pos != std::string::npos)
		{
			size_t eol = source.find_first_of("\r\n", pos);
			CANDY_CORE_ASSERT(eol != std::string::npos, "Syntax error");
			size_t begin = pos + typeTokenLength + 1;
			std::string type = source.substr(begin, eol - begin);

			ShaderStage stage = ShaderStageFromString(type);
			CANDY_CORE_ASSERT(stage != ShaderStage::None, "Invalid shader type specified");

			size_t nextLinePos = source.find_first_not_of("\r\n", eol);
			CANDY_CORE_ASSERT(nextLinePos != std::string::npos, "Syntax error");
			pos = source.find(typeToken, nextLinePos);

			shaderSources[stage] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
		}

		return shaderSources;
	}

} // namespace Candy
