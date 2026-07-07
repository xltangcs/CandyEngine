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

-- Python3 自动检测（PATH 中的 python，版本 ≥ 3.11）
local function trim(s)
    return s and s:match("^(.-)%s*$") or ""
end

local pyProbe = os.outputof("python -c \"import sys; v=sys.version_info; print(v.major); print(v.minor); print(sys.exec_prefix)\"")

local pyMajor, pyMinor, pyPrefix = nil, nil, nil
if pyProbe and pyProbe ~= "" then
    local lines = {}
    for line in pyProbe:gmatch("([^\n]+)") do
        table.insert(lines, trim(line))
    end
    pyMajor = tonumber(lines[1])
    pyMinor = tonumber(lines[2])
    pyPrefix = lines[3]
end

if pyMajor == 3 and pyMinor >= 11 then
    IncludeDir["Python3"] = pyPrefix .. "/include"
    PythonLibDir = pyPrefix .. "/libs"
    PythonLibName = "python" .. pyMajor .. pyMinor .. ".lib"
    print("  Python 3." .. pyMinor .. " detected: " .. pyPrefix)
else
    if pyMajor then
        print("WARNING: Python " .. pyMajor .. "." .. pyMinor .. " < 3.11, scripting disabled")
    else
        print("WARNING: Python not found in PATH, scripting disabled")
    end
    IncludeDir["Python3"] = ""
    PythonLibDir = ""
    PythonLibName = ""
end


group "Dependencies"
	include "Candy/ThirdParty/GLFW"
	include "Candy/ThirdParty/box2d"
	include "Candy/ThirdParty/GLAD"
	include "Candy/ThirdParty/imgui"
	include "Candy/ThirdParty/yaml-cpp"
group ""

include "Candy"
include "Candy_Editor"
--include "Sandbox"