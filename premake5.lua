include "./vendor/premake/premake_customization/solution_items.lua"

workspace "Hazel"
	architecture("x86_64")
	startproject "Hazelnut"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	solution_items
	{
		".editorconfig"
	}

	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relative to root folder (solution directory) 
IncludeDir = { }
IncludeDir["GLFW"] = "%{wks.location}/Hazel/vendor/GLFW/include"
IncludeDir["GLAD"] = "%{wks.location}/Hazel/vendor/GLAD/include"
IncludeDir["Imgui"] = "%{wks.location}/Hazel/vendor/imgui"
IncludeDir["glm"] = "%{wks.location}/Hazel/vendor/glm"
IncludeDir["stb_image"] = "%{wks.location}/Hazel/vendor/stb_image"
IncludeDir["entt"] = "%{wks.location}/Hazel/vendor/entt/include"
IncludeDir["yaml_cpp"] = "%{wks.location}/Hazel/vendor/yaml-cpp/include"
IncludeDir["ImGuizmo"] = "%{wks.location}/Hazel/vendor/ImGuizmo"
IncludeDir["Box2D"] = "%{wks.location}/Hazel/vendor/Box2D/include"
IncludeDir["mono"] = "%{wks.location}/Hazel/vendor/mono/include"

LibraryDir = {}
LibraryDir["mono"] = "%{wks.location}/Hazel/vendor/mono/lib/%{cfg.buildcfg}"

Library = {}
Library["mono"] = "%{LibraryDir.mono}/libmono-static-sgen.lib"

-- Windows
Library["WinSock"] = "Ws2_32.lib"
Library["WinMM"] = "Winmm.lib"
Library["WinVersion"] = "Version.lib"
Library["BCrypt"] = "Bcrypt.lib"

group "Dependencies"
	include "vendor/premake"
	include "Hazel/vendor/Box2D"
	include "Hazel/vendor/GLFW"
	include "Hazel/vendor/GLAD"
	include "Hazel/vendor/imgui"
	include "Hazel/vendor/yaml-cpp"
group ""

group "Core"
	include "Hazel"
	include "Hazel-ScriptCore"
group ""

group "Tools"
	include "Hazelnut"
group ""

group "Misc"
	include "Sandbox"
group ""
