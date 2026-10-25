workspace("GameBoyEmulator")
configurations({ "Debug", "Release" })
architecture("x86_64")

local SDL = "vendor/SDL"

-- SDL3 is always built from source (vendor/SDL) and linked statically:
--   Windows: compiled straight by premake. The file list is read from SDL's
--            own Visual Studio project, so it always matches whichever SDL
--            version is in vendor/SDL.
--   Linux:   built by SDL's own CMake project (needs `cmake` installed),
--            because SDL needs a generated build config on Linux.
local function sdlWindowsSources()
    local vcxproj = SDL .. "/VisualC/SDL/SDL.vcxproj"
    local text = io.readfile(vcxproj)
    if not text then
        error("Could not read " .. vcxproj .. " - is SDL checked out in " .. SDL .. "?")
    end

    local sources = {}
    for rel in text:gmatch('<ClCompile Include="%.%.\\%.%.\\([^"]+)"') do
        table.insert(sources, SDL .. "/" .. (rel:gsub("\\", "/")))
    end
    if #sources == 0 then
        error("No source files found in " .. vcxproj)
    end
    return sources
end

local function useSDL()
    filter({})
    includedirs({ SDL .. "/include" })

    filter("system:windows")
    links({
        "SDL3-static",
        "kernel32", "user32", "gdi32", "winmm", "imm32", "ole32",
        "oleaut32", "version", "uuid", "advapi32", "setupapi", "shell32",
    })

    filter("system:linux")
    dependson({ "SDL3-static" }) -- build SDL first
    libdirs({ "lib/%{cfg.buildcfg}" })
    links({ "SDL3", "m", "pthread", "dl" })

    filter({})
end

if os.target() == "windows" then
    project("SDL3-static")
    kind("StaticLib")
    language("C++") -- SDL has a few .cpp files; .c files are still compiled as C
    targetdir("lib/%{cfg.buildcfg}")
    warnings("Off")

    includedirs({
        SDL .. "/include",
        SDL .. "/include/build_config",
        SDL .. "/src",
        SDL .. "/src/video/khronos", -- bundled EGL / GLES / Vulkan headers
    })

    defines({ "SDL_STATIC_LIB", "_WINDOWS" })

    files(sdlWindowsSources())

    filter("configurations:Debug")
    defines({ "_DEBUG" })
    symbols("On")

    filter("configurations:Release")
    defines({ "NDEBUG" })
    optimize("On")

    filter({})
else
    -- Linux: run SDL's CMake build as a makefile project.
    -- The static library ends up in lib/<config>/libSDL3.a.
    if not os.isfile(SDL .. "/CMakeLists.txt") then
        error("Could not find " .. SDL .. "/CMakeLists.txt - is SDL checked out in " .. SDL .. "?")
    end

    local srcDir = '"%{wks.location}/' .. SDL .. '"'
    local buildDir = '"%{wks.location}/obj/SDL3-cmake/%{cfg.buildcfg}"'
    local libDir = '"%{wks.location}/lib/%{cfg.buildcfg}"'

    -- Only configure the first time; later builds just rerun `cmake --build`.
    local configure = "test -f " .. buildDir .. "/CMakeCache.txt || " .. table.concat({
        "cmake -S " .. srcDir .. " -B " .. buildDir,
        "-DCMAKE_BUILD_TYPE=%{cfg.buildcfg}",
        "-DSDL_STATIC=ON -DSDL_SHARED=OFF",
        "-DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_INSTALL=OFF",
    }, " ")

    project("SDL3-static")
    kind("Makefile")

    local makeLibDir = "cmake -E make_directory " .. libDir
    local copyLib = "cmake -E copy_if_different " .. buildDir .. "/libSDL3.a " .. libDir

    buildcommands({
        configure,
        "cmake --build " .. buildDir .. " --parallel 8",
        makeLibDir,
        copyLib,
    })
    rebuildcommands({
        configure,
        "cmake --build " .. buildDir .. " --clean-first --parallel 8",
        makeLibDir,
        copyLib,
    })
    cleancommands({
        "cmake -E remove_directory " .. buildDir,
        "cmake -E remove -f " .. libDir .. "/libSDL3.a",
    })

    filter({})
end

project("GameBoyEmulator")
kind("ConsoleApp")
language("C++")
cppdialect("C++23")
targetdir("bin/%{cfg.buildcfg}")

files({
    "src/**.h",
    "src/**.hpp",
    "src/**.cpp",
})

includedirs({
    "src",
    "include",
})

filter("configurations:Debug")
symbols("On")

filter("configurations:Release")
optimize("On")

useSDL()

project("gb_tests")
kind("ConsoleApp")
language("C++")
cppdialect("C++23")
targetdir("bin/%{cfg.buildcfg}")

files({
    "tests/**.cpp",
    "src/**.h",
    "src/**.hpp",
    "src/**.cpp",
})

removefiles({
    "src/main.cpp", -- exclude the real emulator's entry point so main() doesn't collide
    "tests/dmg_acid2_test_main.cpp",
})

includedirs({
    "src",
    "include",
    "tests",
})

filter("configurations:Debug")
symbols("On")

filter("configurations:Release")
optimize("On")

useSDL()

project("gb_acid2")
kind("ConsoleApp")
language("C++")
cppdialect("C++23")
targetdir("bin/%{cfg.buildcfg}")

files({
    "tests/dmg_acid2_test_main.cpp",
    "src/**.h",
    "src/**.hpp",
    "src/**.cpp",
})

removefiles({
    "src/main.cpp",
})

includedirs({
    "src",
    "include",
    "tests",
})

filter("configurations:Debug")
symbols("On")

filter("configurations:Release")
optimize("On")

useSDL()
