workspace "CandyEngine"		-- sln文件名
	architecture "x64"	
	startproject "Candy_Editor"
	configurations{
		"Debug",
		"Release",
		"Dist"
	}

	flags
	{
		"MultiProcessorCompile"
	}
-- outputdir = Debug-windows-x86_64
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["GLFW"]  = "Candy/vendor/GLFW/include"
IncludeDir["Glad"]  = "Candy/vendor/Glad/include"
IncludeDir["Imgui"] = "Candy/vendor/imgui"
IncludeDir["glm"]   = "Candy/vendor/glm"
IncludeDir["stb_image"] = "Candy/vendor/stb_image"

group "Dependencies"
	include "Candy/vendor/GLFW"
	include "Candy/vendor/Glad"
	include "Candy/vendor/imgui"

group ""


project "Candy"		
	location "Candy"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}") 
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    pchheader "candypch.h"
    pchsource "Candy/src/candypch.cpp"

	
	files{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/stb_image/**.h",
		"%{prj.name}/vendor/stb_image/**.cpp",
		"%{prj.name}/vendor/glm/glm/**.hpp",
		"%{prj.name}/vendor/glm/glm/**.inl"
	}
	defines{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE"
	}
	
	includedirs{
        "%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.Imgui}",
        "%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}"
	}

    links 
	{ 
		"GLFW",
		"Glad",
		"Imgui",
		"opengl32.lib"
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

project "Sandbox"
	location "Sandbox"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}
	
	includedirs{
		"Candy/vendor/spdlog/include",
		"Candy/src",
		"Candy/vendor",
        "%{IncludeDir.glm}"
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

project "Candy_Editor"
	location "Candy_Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}
	
	includedirs{
		"Candy/vendor/spdlog/include",
		"Candy/src",
		"Candy/vendor",
		"%{IncludeDir.glm}"
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