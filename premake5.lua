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


group "Dependencies"
	include "vendor/premake"
	include "Hazel/vendor/GLFW"
	include "Hazel/vendor/GLAD"
	include "Hazel/vendor/imgui"
	include "Hazel/vendor/yaml-cpp"
group ""

include "Hazel"
include "Sandbox"
include "Hazelnut"