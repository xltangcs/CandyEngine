project "Candy"		
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    pchheader "CandyPCH.h"
    pchsource "Source/CandyPCH.cpp"

	
	files{
		"Source/Candy.h",
		"Source/CandyPCH.h",
		"Source/CandyPCH.cpp",
		"Source/Platform/**.h",
		"Source/Platform/**.cpp",
		"Source/Runtime/**.h",
		"Source/Runtime/**.cpp",
		"ThirdParty/stb_image/**.h",
		"ThirdParty/stb_image/**.cpp",
		"ThirdParty/glm/glm/**.hpp",
		"ThirdParty/glm/glm/**.inl",
		"ThirdParty/ImGuizmo/src/ImGuizmo.h",
		"ThirdParty/ImGuizmo/src/ImGuizmo.cpp",
		"ThirdParty/miniaudio/**.h",
		"ThirdParty/miniaudio/**.cpp"
	}
	defines{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE",
		"YAML_CPP_STATIC_DEFINE"
	}

	buildoptions { "/utf-8" }
	
	includedirs{
        "Source",
        "Source/Runtime",
		"ThirdParty/spdlog/include",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.GLAD}",
        "%{IncludeDir.Imgui}",
        "%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.box2d}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.pybind11}",
		"%{IncludeDir.Python3}",
		"ThirdParty/miniaudio"
	}

    links 
	{ 
		"GLFW",
		"GLAD",
		"Imgui",
		"yaml-cpp",
		"box2d",
		"opengl32.lib"
	}

	if PythonLibDir and PythonLibDir ~= "" then
		links { PythonLibDir .. "/" .. PythonLibName }
	end

	filter "files:ThirdParty/ImGuizmo/src/**.cpp"
	flags { "NoPCH" }

	filter "files:Source/Runtime/Scripting/**.cpp"
	flags { "NoPCH" }
	buildoptions { "/bigobj" }

	filter "files:Source/Runtime/Audio/**.cpp"
	flags { "NoPCH" }

	filter "files:ThirdParty/miniaudio/**.cpp"
	flags { "NoPCH" }

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

project "CandyEditor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"
	buildoptions { "/utf-8" }
	defines { "YAML_CPP_STATIC_DEFINE" }

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files{
		"Source/Editor/**.h",
		"Source/Editor/**.cpp"
	}
	
	includedirs{
		"%{wks.location}/Candy/ThirdParty/spdlog/include",
		"%{wks.location}/Candy/Source",
		"%{wks.location}/Candy/Source/Editor",
		"%{wks.location}/Candy/ThirdParty",

		"%{IncludeDir.glm}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.GLFW}"
	}
	
	links{
		"Candy"
	}

	if PythonLibDir and PythonLibDir ~= "" then
		links { PythonLibDir .. "/" .. PythonLibName }

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