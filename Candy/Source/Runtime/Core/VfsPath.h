#pragma once

#include <string>
#include <algorithm>

namespace Candy {

	// VfsPath — value type for the unified VFS:// path scheme.
	// Examples:
	//   "VFS://Engine/Icons/StopButton.png"  -> { Engine, "Icons/StopButton.png" }
	//   "VFS://Game/Scripts/Player.py"       -> { Game,   "Scripts/Player.py" }
	//   "VFS://Engine"                       -> { Engine, "" }  (domain root)
	struct VfsPath
	{
		enum class Domain { Engine, Game, Invalid };

		Domain domain = Domain::Invalid;
		std::string relativePath;  // no prefix, forward-slash separated, e.g. "Scripts/Player.py"

		VfsPath() = default;
		VfsPath(Domain d, const std::string& rel) : domain(d), relativePath(rel) {}

		// Parse a "VFS://Engine/..." or "VFS://Game/..." string. Returns Invalid on failure.
		static VfsPath Parse(const std::string& s)
		{
			const std::string scheme = "VFS://";
		if (!s.starts_with(scheme))
			return VfsPath{};

			std::string rest = s.substr(scheme.size());  // "Engine/Icons/x.png" or "Game/..."

			const std::string enginePrefix = "Engine/";
			const std::string gamePrefix = "Game/";
			const std::string engineOnly = "Engine";
			const std::string gameOnly = "Game";

			if (rest.starts_with(enginePrefix))
				return VfsPath{ Domain::Engine, rest.substr(enginePrefix.size()) };
			if (rest.starts_with(gamePrefix))
				return VfsPath{ Domain::Game, rest.substr(gamePrefix.size()) };
			if (rest == engineOnly)
				return VfsPath{ Domain::Engine, "" };
			if (rest == gameOnly)
				return VfsPath{ Domain::Game, "" };

			return VfsPath{};
		}

		static VfsPath FromEngine(const std::string& rel) { return VfsPath{ Domain::Engine, rel }; }
		static VfsPath FromGame(const std::string& rel) { return VfsPath{ Domain::Game, rel }; }

		bool IsValid() const { return domain != Domain::Invalid; }

		std::string DomainLabel() const
		{
			switch (domain)
			{
				case Domain::Engine: return "Engine";
				case Domain::Game:   return "Game";
				default:             return "Invalid";
			}
		}

		// "VFS://Game/Scripts/x.py" (or "VFS://Engine" for root)
		std::string ToString() const
		{
			std::string s = "VFS://" + DomainLabel();
			if (!relativePath.empty())
				s += "/" + relativePath;
			return s;
		}
	};

}
