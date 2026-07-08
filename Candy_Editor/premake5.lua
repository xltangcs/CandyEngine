project "Candy_Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"
	buildoptions { "/utf-8" }

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
		"%{IncludeDir.ImGuizmo}"
	}
	
	links{
		"Candy"
	}

	if PythonLibDir and PythonLibDir ~= "" then
		links { PythonLibDir .. "/" .. PythonLibName }
	end

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