add_rules("mode.debug", "mode.release")

set_languages("c++23", "c17")

if is_plat("linux") then
    -- The main branch is currently not compatible with gcc when compiling as
    -- C++20 or above.
    add_requires("websocketpp develop")
else
    add_requires("websocketpp");
end
add_requires("spdlog");
add_requires("nlohmann_json");

if is_mode("debug") then
    add_defines("DEBUG")
end

set_allowedarchs("x64", "x86_64")
set_allowedplats("windows", "linux")

if is_plat("windows") then
    add_cxxflags("/EHsc")
end

target("baseqware-server")
    set_kind("binary")
    add_files("src/*.cpp", "src/sdk/*.cpp")
    add_packages("websocketpp", "spdlog", "nlohmann_json")
    if is_plat("windows") then
        add_files("windows/*.c", "windows/*.cpp")
        add_links("Advapi32")
    end
    if is_plat("linux") then
        add_files("linux/*.cpp")
        add_cxflags("-pthread")
    end
