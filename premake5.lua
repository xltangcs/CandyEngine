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
IncludeDir["ImGuizmo"]  = "%{wks.location}/Candy/vendor/ImGuizmo"
IncludeDir["box2d"]  = "%{wks.location}/Candy/vendor/box2d/include"


group "Dependencies"
	include "Candy/vendor/GLFW"
	include "Candy/vendor/box2d"
	include "Candy/vendor/Glad"
	include "Candy/vendor/imgui"
	include "Candy/vendor/yaml-cpp"
group ""

include "Candy"
include "Candy_Editor"
--include "Sandbox"