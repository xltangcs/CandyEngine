workspace "CandyEngine"		-- sln文件名
	architecture "x64"	
	startproject "Sandbox"
	configurations{
		"Debug",
		"Release",
		"Dist"
	}
-- https://github.com/premake/premake-core/wiki/Tokens#value-tokens
-- 组成输出目录:Debug-windows-x86_64
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relative to root folder (solution directory)
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


project "Candy"		--Candy项目
	location "Candy"--在sln所属文件夹下的Candy文件夹
	kind "StaticLib"--dll动态库
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}") -- 输出目录
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")-- 中间目录

    pchheader "candypch.h"
    pchsource "Candy/src/candypch.cpp"

	-- 包含的所有h和cpp文件
	files{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/stb_image/**.h",
		"%{prj.name}/vendor/stb_image/**.cpp",
		"%{prj.name}/vendor/glm/glm/**.hpp",
		"%{prj.name}/vendor/glm/glm/**.inl"
	}
	defines{
		"_CRT_SECURE_NO_WARNINGS"
	}
	-- 包含目录
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

	-- 如果是window系统
	filter "system:windows"
		systemversion "latest"	-- windowSDK版本

		-- 预处理器定义
		defines{
			"CANDY_PLATFORM_WINDOWS",
			"CANDY_BUILD_DLL",
			"GLFW_INCLUDE_NONE"
		}

	-- 不同配置下的预定义不同
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
	-- 同样包含spdlog头文件
	includedirs{
		"Candy/vendor/spdlog/include",
		"Candy/src",
		"Candy/vendor",
        "%{IncludeDir.glm}"
	}
	-- 引用hazel
	links{
		"Candy"
	}

	filter "system:windows"
		systemversion "latest"

		defines{
			"CANDY_PLATFORM_WINDOWS"
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
