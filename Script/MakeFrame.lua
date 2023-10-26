function MakeAssignment(projectName)
	assignmentPath = path.join(AssignmentsPath, projectName)
	print("Generating "..projectName.."...")
	print("Source path: "..assignmentPath)
	
	project(projectName)
		kind("ConsoleApp")
		-- SharedLib, StaticLib, ConsoleApp
		language("C++")
		cppdialect("C++20")
		
		-- Intermediate and binary path.
		location(IntermediatePath)
		objdir(path.join(IntermediatePath, "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"))
		targetdir(path.join(BinaryPath, "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"))
		
		-- Target name.
		filter { "configurations:Debug" }
			targetname("%{prj.name}".."d")
		filter { "configurations:Release" }
			targetname("%{prj.name}")
		filter {}
		
		-- Set definitions.
		defines {
			"FRAME_ASSET_PATH=\""..AssetPath.."\"",
		}
		
		-- Set files.
		files {
			path.join(assignmentPath, "**.*"),
			path.join(ThirdPartyPath, "eigen3/Eigen/**.*"),
		}
		
		-- Set filter.
		vpaths {
			["Source/"] = { 
				path.join(assignmentPath, "**.*"),
			},
		}
		
		-- Set include paths.
		includedirs {
			FramePath,
			SourcePath,
			AssignmentsPath,
			assignmentPath,
			ThirdPartyPath,
			path.join(ThirdPartyPath, "opencv/include"),
			path.join(ThirdPartyPath, "opencv/build"),
			path.join(ThirdPartyPath, "opencv/modules/**/include"),
			AssetPath,
		}
		
		-- Link to thirdparty libs.
		filter { "configurations:Debug" }
			libdirs {
				path.join(ThirdPartyPath, "opencv/build/lib/Debug"),
			}
			links {
				"opencv_world481d",
			}
		filter { "configurations:Release" }
			libdirs {
				path.join(ThirdPartyPath, "opencv/build/lib/Release"),
			}
			links {
				"opencv_world481",
			}
		filter {}
		
		-- Use /MT and /MTd.
		staticruntime "on"
		filter { "configurations:Debug" }
			runtime("Debug") -- /MTd
		filter { "configurations:Release" }
			runtime("Release") -- /MT
		filter {}
		
		-- Disable these options can reduce the size of compiled binaries.
		justmycode("Off")
		editAndContinue("Off")
		exceptionhandling("Off")
		rtti("Off")
		
		-- Strict.
		warnings("Default")
		externalwarnings("Off")
		
		flags {
			-- Compiler uses multiple thread.
			"MultiProcessorCompile",
		}
		
		filter { "configurations:Debug" }
			postbuildcommands {
				"{COPY} "..path.join(ThirdPartyPath, "opencv/build/bin/Debug/*.dll".." %{cfg.targetdir}"),
				"{COPY} "..path.join(ThirdPartyPath, "opencv/build/bin/Debug/*.pdb".." %{cfg.targetdir}"),
			}
		filter { "configurations:Release" }
			postbuildcommands {
				"{COPY} "..path.join(ThirdPartyPath, "opencv/build/bin/Release/*.dll".." %{cfg.targetdir}"),
				"{COPY} "..path.join(ThirdPartyPath, "opencv/build/bin/Release/*.pdb".." %{cfg.targetdir}"),
			}
		filter {}
	
print("")
end

MakeAssignment("Assignment0")
MakeAssignment("Assignment1")
MakeAssignment("Assignment2")
MakeAssignment("Assignment3")
MakeAssignment("Assignment4")
