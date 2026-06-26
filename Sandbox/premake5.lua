project "Sandbox"
	location "Sandbox"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files{
		"%{prj.name}/Source/**.h",
		"%{prj.name}/Source/**.cpp"
	}
	
	includedirs{
		"Candy/ThirdParty/spdlog/include",
		"Candy/Source",
		"Candy/ThirdParty",
        "%{IncludeDir.glm}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.yaml_cpp}"
	}
	
	links{
		"Candy"
	}

	filter "system:windows"
		systemversion "latest"

		defines{
			
		}

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