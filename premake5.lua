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
IncludeDir["GLFW"]  	= "%{wks.location}/Candy/vendor/GLFW/include"
IncludeDir["Glad"]  	= "%{wks.location}/Candy/vendor/Glad/include"
IncludeDir["Imgui"] 	= "%{wks.location}/Candy/vendor/imgui"
IncludeDir["glm"]   	= "%{wks.location}/Candy/vendor/glm"
IncludeDir["stb_image"] = "%{wks.location}/Candy/vendor/stb_image"
IncludeDir["entt"]  	= "%{wks.location}/Candy/vendor/entt/include"
IncludeDir["yaml_cpp"]  = "%{wks.location}/Candy/vendor/yaml-cpp/include"


group "Dependencies"
	include "Candy/vendor/GLFW"
	include "Candy/vendor/Glad"
	include "Candy/vendor/imgui"
	include "Candy/vendor/yaml-cpp"
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
		"GLFW_INCLUDE_NONE",
		"YAML_CPP_STATIC_DEFINE"
	}
	
	includedirs{
        "%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.Imgui}",
        "%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.entt}"
		
	}

    links 
	{ 
		"GLFW",
		"Glad",
		"Imgui",
		"yaml-cpp",
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