add_rules("mode.debug", "mode.release")

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
    set_languages("c++20")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("websocketpp", "spdlog", "nlohmann_json")
