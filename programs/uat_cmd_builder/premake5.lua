-- Some global constants for common folder locations
local GAME_PIPE_ROOT = "../../";
local WORKSPACE_DIR = "./workspaces/" -- path to IDE projects
local BUILD_DIR = "./build/" -- path where all artifacts are sent to

local SRC_DIR = "./src/";
local LIB_DIR = "./libs/";
local GAME_PIPE_SHARED_LIBS = GAME_PIPE_ROOT .. "/shared_libs/";

local SDL2_DIR = os.getenv("SDL2_DIR");
local WIN_SDK_DIR = os.getenv("WIN_SDK_DIR");
local WIN_SDK_VERSION = "10.0.17763.0";

workspace "UATCmdBuilder"
    location(WORKSPACE_DIR .. _ACTION) -- a unique directory for each IDE solution
    configurations { "Debug", "FastDebug", "Release" }
    platforms { "Win64" }

    filter { "platforms:Win64" }
        system "Windows"
        architecture "x64"

    filter {}

project "UATCmdBuilder"
    targetname "uat_cmd_builder"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++11"
    systemversion "latest" -- use latest windows SDK
    targetdir(BUILD_DIR .. "/uat_cmd_builder/%{cfg.buildcfg}")
    
    local WORKING_DIR = WORKSPACE_DIR .. _ACTION .. "/debug_dir";
    
    debugdir(WORKING_DIR)

    filter "platforms:x64"
        architecture "x64"

    filter "configurations:*"
        postbuildmessage "Copying SDL2.dll into ./debug_dir"
        postbuildcommands 
        { 
            "mkdir debug_dir",
            "{copy} " .. SDL2_DIR .. "/lib/x64/SDL2.dll ./debug_dir/" 
        }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "Full"
        optimize "Off"
        staticruntime "On"

    filter "configurations:FastDebug"
        defines { "FAST_DEBUG" }
        symbols "Full"
        staticruntime "On"
        optimize "On"
    
    filter "configurations:Release"
        defines { "NDEBUG", "RELEASE" }
        symbols "On"
        staticruntime "On"
        optimize "Full"

    filter {}

-----------------------------
-- Add sourcetree to project
-----------------------------

-- project sources
files 
{
    SRC_DIR .. "**.h",
    SRC_DIR .. "**.cpp",
    -- add dear imgui sources
    GAME_PIPE_SHARED_LIBS .. "dear_imgui/**.h",
    GAME_PIPE_SHARED_LIBS .. "dear_imgui/**.cpp"
}

includedirs
{
    SDL2_DIR .. "/include",
    WIN_SDK_DIR .. "/Include/" .. WIN_SDK_VERSION .. "/um",
    WIN_SDK_DIR .. "/Include/" .. WIN_SDK_VERSION .. "/shared"
}

libdirs
{
    SDL2_DIR .. "/lib/x64",
    WIN_SDK_DIR .. "/Libs/" .. WIN_SDK_VERSION .. "/um/x64"
}

links
{
    "SDL2.lib",
    "SDL2main.lib",
    "d3d11.lib",
    "d3dcompiler.lib"
}

-- Create a custom clean workspaces action (there's no premake default for this)
newaction 
{
    trigger = "clean",
    description = "Cleans the workspace folder, deletes all platform-dependent generated build system files and deletes all artifacts",
    execute = function ()
        print "Remove artifacts and generated build files under ./workspaces/*"
        os.rmdir "./workspaces"
        os.rmdir "./build"
        print "Finished cleanup!"
    end
}