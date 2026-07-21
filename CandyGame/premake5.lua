project "CandyGame"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"
	buildoptions { "/utf-8" }
	defines { "YAML_CPP_STATIC_DEFINE" }

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files{
		"Source/**.h",
		"Source/**.cpp"
	}
	
	includedirs{
		"%{wks.location}/Candy/ThirdParty/spdlog/include",
		"%{wks.location}/Candy/Source",
		"%{wks.location}/Candy/ThirdParty",

		"%{IncludeDir.glm}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.GLFW}"
	}
	
	links{
		"Candy"
	}

	if PythonLibDir and PythonLibDir ~= "" then
		links { PythonLibDir .. "/" .. PythonLibName }

		-- Post-build: copy embedded Python runtime to output directory
		postbuildcommands {
			'{COPY} "%{wks.location}/Candy/ThirdParty/Python3/python314.dll" "%{cfg.targetdir}"',
			'{COPY} "%{wks.location}/Candy/ThirdParty/Python3/python314.zip" "%{cfg.targetdir}"',
			'{COPY} "%{wks.location}/Candy/ThirdParty/Python3/python314._pth" "%{cfg.targetdir}"',
		}
	end

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "CANDY_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "CANDY_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "CANDY_DIST"
		runtime "Release"
		optimize "on"
