project "Candy_Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files{
		"src/**.h",
		"src/**.cpp"
	}
	
	includedirs{
		"%{wks.location}/Candy/vendor/spdlog/include",
		"%{wks.location}/Candy/src",
		"%{wks.location}/Candy/vendor",

		"%{IncludeDir.glm}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.ImGuizmo}"
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