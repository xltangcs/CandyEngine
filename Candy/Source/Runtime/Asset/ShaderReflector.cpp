#include "CandyPCH.h"

#include "Runtime/Asset/ShaderReflector.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace Candy {

	namespace {

		std::string Trim(const std::string& s)
		{
			const size_t begin = s.find_first_not_of(" \t\r\n");
			if (begin == std::string::npos)
				return {};
			const size_t end = s.find_last_not_of(" \t\r\n");
			return s.substr(begin, end - begin + 1);
		}

		std::string ToLower(std::string s)
		{
			std::transform(s.begin(), s.end(), s.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}

		bool StartsWith(const std::string& s, const std::string& prefix)
		{
			return s.rfind(prefix, 0) == 0;
		}

		// Matches a keyword only when it is not part of a longer identifier,
		// e.g. "color" matches but "colorful" does not.
		bool MatchKeyword(const std::string& s, const std::string& keyword)
		{
			if (!StartsWith(s, keyword))
				return false;
			if (s.size() == keyword.size())
				return true;
			const char next = s[keyword.size()];
			return !(std::isalnum(static_cast<unsigned char>(next)) || next == '_');
		}

		// Position of the "@param" marker inside a `//@param` / `// @param` comment.
		size_t FindParamMarker(const std::string& line)
		{
			const size_t a = line.find("//@param");
			if (a != std::string::npos)
				return a + 2;
			const size_t b = line.find("// @param");
			if (b != std::string::npos)
				return b + 3;
			return std::string::npos;
		}

		// Reads a parenthesized block starting after `pos`, e.g. for
		// "range(0, 1, 0.01)" returns "0, 1, 0.01" and advances pos past ')'.
		std::string ReadParenBlock(const std::string& s, size_t& pos)
		{
			const size_t open = s.find('(', pos);
			if (open == std::string::npos)
				return {};
			const size_t close = s.find(')', open);
			if (close == std::string::npos)
				return {};
			pos = close + 1;
			return s.substr(open + 1, close - open - 1);
		}

		std::vector<std::string> Split(const std::string& s, char delim)
		{
			std::vector<std::string> parts;
			std::stringstream ss(s);
			std::string item;
			while (std::getline(ss, item, delim))
				parts.push_back(Trim(item));
			return parts;
		}

		bool TryParseFloat(const std::string& s, float& out)
		{
			if (s.empty())
				return false;
			char* end = nullptr;
			out = std::strtof(s.c_str(), &end);
			return end && *end == '\0';
		}

		bool TryParseInt(const std::string& s, int& out)
		{
			if (s.empty())
				return false;
			char* end = nullptr;
			out = static_cast<int>(std::strtol(s.c_str(), &end, 10));
			return end && *end == '\0';
		}

		// Extracts `type name` tokens from a declaration line, tolerating
		// "const" prefixes, array suffixes and ": register(...)" layout clauses.
		bool ParseDeclaration(const std::string& decl, std::string& outType, std::string& outName)
		{
			std::stringstream ss(decl);
			std::string token;
			std::string typeTok, nameTok;
			int tokenIndex = 0;
			while (ss >> token)
			{
				if (tokenIndex == 0 && ToLower(token) == "const")
					continue;
				if (tokenIndex == 0)
				{
					typeTok = token;
					tokenIndex++;
					continue;
				}
				if (tokenIndex == 1)
				{
					nameTok = token;
					break;
				}
			}
			if (typeTok.empty() || nameTok.empty())
				return false;

			const size_t bracket = nameTok.find('[');
			const size_t colon = nameTok.find(':');
			const size_t cut = std::min(
				bracket == std::string::npos ? nameTok.size() : bracket,
				colon == std::string::npos ? nameTok.size() : colon);
			outName = nameTok.substr(0, cut);
			while (!outName.empty() && outName.back() == ';')
				outName.pop_back();
			if (outName.empty())
				return false;

			outType = ToLower(typeTok);
			return true;
		}

		bool ResolveType(const std::string& typeTok, ShaderParamType& outType)
		{
			if (typeTok == "float")                                        { outType = ShaderParamType::Float; return true; }
			if (typeTok == "int")                                          { outType = ShaderParamType::Int;   return true; }
			if (typeTok == "bool")                                         { outType = ShaderParamType::Bool;  return true; }
			if (typeTok == "float2" || typeTok == "vec2")                  { outType = ShaderParamType::Vec2;  return true; }
			if (typeTok == "float3" || typeTok == "vec3")                  { outType = ShaderParamType::Vec3;  return true; }
			if (typeTok == "float4" || typeTok == "vec4")                  { outType = ShaderParamType::Vec4;  return true; }
			if (typeTok == "texture2d" || typeTok == "sampler2d")          { outType = ShaderParamType::Texture; return true; }
			return false;
		}

		bool ParseDefaultValue(const std::string& text, ShaderParameter& param)
		{
			const std::string v = Trim(text);

			if (param.IsEnum)
			{
				// Label or numeric index.
				int index = 0;
				if (TryParseInt(v, index))
				{
					param.Default = index;
					return true;
				}
				for (size_t i = 0; i < param.EnumLabels.size(); ++i)
				{
					if (param.EnumLabels[i] == v)
					{
						param.Default = static_cast<int>(i);
						return true;
					}
				}
				param.Default = 0;
				return true;
			}

			switch (param.Type)
			{
				case ShaderParamType::Texture:
				{
					std::string path = v;
					if (path.size() >= 2 && path.front() == '"' && path.back() == '"')
						path = path.substr(1, path.size() - 2);
					param.Default = path;
					return true;
				}
				case ShaderParamType::Float:
				{
					float f = 0.0f;
					if (!TryParseFloat(v, f))
						return false;
					param.Default = f;
					return true;
				}
				case ShaderParamType::Int:
				{
					int n = 0;
					if (!TryParseInt(v, n))
						return false;
					param.Default = n;
					return true;
				}
				case ShaderParamType::Bool:
				{
					const std::string lower = ToLower(v);
					if (lower == "true")
					{
						param.Default = true;
						return true;
					}
					if (lower == "false")
					{
						param.Default = false;
						return true;
					}
					int n = 0;
					if (TryParseInt(v, n))
					{
						param.Default = (n != 0);
						return true;
					}
					return false;
				}
				case ShaderParamType::Vec2:
				case ShaderParamType::Vec3:
				case ShaderParamType::Vec4:
				case ShaderParamType::Color:
				{
					// "(x, y[, z[, w]])"
					const size_t open = v.find('(');
					const size_t close = v.find(')');
					if (open == std::string::npos || close == std::string::npos || close < open)
						return false;
					const std::string body = v.substr(open + 1, close - open - 1);
					const auto nums = Split(body, ',');
					const size_t required = (param.Type == ShaderParamType::Vec2) ? 2
						: (param.Type == ShaderParamType::Vec3) ? 3 : 4;
					if (nums.size() < required)
						return false;
					float vals[4] = {};
					for (size_t k = 0; k < required; ++k)
					{
						if (!TryParseFloat(nums[k], vals[k]))
							return false;
					}
					if (param.Type == ShaderParamType::Vec2)
						param.Default = glm::vec2(vals[0], vals[1]);
					else if (param.Type == ShaderParamType::Vec3)
						param.Default = glm::vec3(vals[0], vals[1], vals[2]);
					else
						param.Default = glm::vec4(vals[0], vals[1], vals[2], vals[3]);
					return true;
				}
				default:
					return false;
			}
		}

		ShaderParamValue TypeDefaultValue(ShaderParamType type)
		{
			switch (type)
			{
				case ShaderParamType::Float:   return 0.0f;
				case ShaderParamType::Int:     return 0;
				case ShaderParamType::Bool:    return false;
				case ShaderParamType::Vec2:    return glm::vec2(0.0f);
				case ShaderParamType::Vec3:    return glm::vec3(0.0f);
				case ShaderParamType::Vec4:    return glm::vec4(0.0f);
				case ShaderParamType::Color:   return glm::vec4(1.0f);
				case ShaderParamType::Texture: return std::string();
				default:                       return 0.0f;
			}
		}

		bool ParseParameter(const std::string& decl, std::string spec, ShaderParameter& out)
		{
			std::string typeTok, name;
			if (!ParseDeclaration(decl, typeTok, name))
				return false;

			ShaderParamType type;
			if (!ResolveType(typeTok, type))
				return false;

			out.Name = name;
			out.DisplayName = name;
			out.Type = type;

			spec = Trim(spec);

			// --- hint -----------------------------------------------------------
			while (!spec.empty())
			{
				if (MatchKeyword(spec, "color"))
				{
					out.Type = ShaderParamType::Color;
					spec = Trim(spec.substr(5));
				}
				else if (MatchKeyword(spec, "texture"))
				{
					out.Type = ShaderParamType::Texture;
					spec = Trim(spec.substr(7));
				}
				else if (MatchKeyword(spec, "range"))
				{
					size_t pos = 0;
					const std::string args = ReadParenBlock(spec, pos);
					if (args.empty())
						return false;
					const auto nums = Split(args, ',');
					out.HasRange = true;
					if (nums.size() > 0 && !nums[0].empty()) TryParseFloat(nums[0], out.Range.Min);
					if (nums.size() > 1 && !nums[1].empty()) TryParseFloat(nums[1], out.Range.Max);
					if (nums.size() > 2 && !nums[2].empty()) TryParseFloat(nums[2], out.Range.Step);
					spec = Trim(spec.substr(pos));
				}
				else if (MatchKeyword(spec, "enum"))
				{
					size_t pos = 0;
					const std::string args = ReadParenBlock(spec, pos);
					if (args.empty())
						return false;
					out.IsEnum = true;
					out.EnumLabels = Split(args, ',');
					spec = Trim(spec.substr(pos));
				}
				else
					break;
			}

			// --- display name -----------------------------------------------------
			if (!spec.empty() && spec.front() == '"')
			{
				const size_t end = spec.find('"', 1);
				if (end != std::string::npos)
				{
					out.DisplayName = spec.substr(1, end - 1);
					spec = Trim(spec.substr(end + 1));
				}
			}

			// --- default value -----------------------------------------------------
			if (!spec.empty() && spec.front() == '=')
			{
				if (!ParseDefaultValue(Trim(spec.substr(1)), out))
					return false;
			}
			else
			{
				out.Default = out.IsEnum ? ShaderParamValue(0) : TypeDefaultValue(out.Type);
			}

			return true;
		}

	} // namespace

	std::vector<ShaderParameter> ShaderReflector::Reflect(const std::string& source)
	{
		std::vector<ShaderParameter> parameters;

		std::vector<std::string> lines;
		{
			std::stringstream ss(source);
			std::string line;
			while (std::getline(ss, line))
				lines.push_back(line);
		}

		for (size_t i = 0; i < lines.size(); ++i)
		{
			const std::string trimmed = Trim(lines[i]);
			const size_t marker = FindParamMarker(trimmed);
			if (marker == std::string::npos)
				continue;

			// The declaration may share the line with the @param comment
			// ("float4 u_X; // @param ...") or live on a previous line.
			std::string decl;
			const size_t semi = trimmed.find(';');
			if (semi != std::string::npos && semi < marker)
			{
				decl = trimmed.substr(0, semi + 1);
			}
			else
			{
				// Backtrack to the nearest declaration line: non-empty, not a
				// comment, not a block closer, and containing ';'.
				for (size_t j = i; j-- > 0;)
				{
					const std::string t = Trim(lines[j]);
					if (t.empty())
						continue;
					if (StartsWith(t, "//") || StartsWith(t, "/*") || StartsWith(t, "*"))
						continue;
					if (StartsWith(t, "}"))
						continue;
					if (t.find(';') == std::string::npos)
						continue;
					decl = t;
					break;
				}
			}
			if (decl.empty())
				continue;

			ShaderParameter param;
			if (!ParseParameter(decl, trimmed.substr(marker + 6), param)) // strlen("@param")
				continue;

			parameters.push_back(std::move(param));
		}

		return parameters;
	}

} // namespace Candy
