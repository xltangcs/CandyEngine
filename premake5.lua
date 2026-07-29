workspace "CandyEngine"		-- sln文件名
	architecture "x64"	
	startproject "CandyEditor"
	
	language "C++"
	cppdialect "C++20"
        
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
IncludeDir["GLFW"]  	= "%{wks.location}/Candy/ThirdParty/GLFW/include"
IncludeDir["GLAD"]  	= "%{wks.location}/Candy/ThirdParty/GLAD/include"
IncludeDir["Imgui"] 	= "%{wks.location}/Candy/ThirdParty/imgui"
IncludeDir["glm"]   	= "%{wks.location}/Candy/ThirdParty/glm"
IncludeDir["stb_image"] = "%{wks.location}/Candy/ThirdParty/stb_image"
IncludeDir["entt"]  	= "%{wks.location}/Candy/ThirdParty/entt/include"
IncludeDir["yaml_cpp"]  = "%{wks.location}/Candy/ThirdParty/yaml-cpp/include"
IncludeDir["ImGuizmo"]  = "%{wks.location}/Candy/ThirdParty/ImGuizmo/src"
IncludeDir["box2d"]  	= "%{wks.location}/Candy/ThirdParty/box2d/include"
IncludeDir["pybind11"] 	= "%{wks.location}/Candy/ThirdParty/pybind11/include"

-- Python3: 使用 ThirdParty/Python3 (embedded)
IncludeDir["Python3"] = "%{wks.location}/Candy/ThirdParty/Python3/include"
PythonLibDir = "%{wks.location}/Candy/ThirdParty/Python3/libs"
PythonLibName = "python314.lib"
print("  Python embedded (ThirdParty/Python3)")

-- Vulkan SDK detection: prefer VULKAN_SDK env var, fall back to bundled headers
VulkanSDKPath = os.getenv("VULKAN_SDK")
if VulkanSDKPath and VulkanSDKPath ~= "" then
	IncludeDir["Vulkan"] = VulkanSDKPath .. "/Include"
	print("  Vulkan SDK (env): " .. VulkanSDKPath)
else
	IncludeDir["Vulkan"] = "%{wks.location}/Candy/ThirdParty/Vulkan"
	print("  Vulkan (bundled headers in ThirdParty/Vulkan)")
end

-- DX12: Windows SDK provides headers and libs automatically


group "Dependencies"
	include "Candy/ThirdParty/GLFW"
	include "Candy/ThirdParty/box2d"
	include "Candy/ThirdParty/GLAD"
	include "Candy/ThirdParty/imgui"
	include "Candy/ThirdParty/yaml-cpp"
group ""

include "Candy"
--include "CandyEditor"
include "CandyGame"
include "PakTool"
--include "Sandbox"