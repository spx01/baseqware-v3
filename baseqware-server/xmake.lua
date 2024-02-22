add_rules("mode.debug", "mode.release")

set_languages("c++23", "c17")

add_requires("websocketpp");
add_requires("spdlog");
add_requires("nlohmann_json");

if is_mode("debug") then
    add_defines("DEBUG")
end

set_allowedarchs("x64")
-- linux support is not yet implemented
set_allowedplats("windows")

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
