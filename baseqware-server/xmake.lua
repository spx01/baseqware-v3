add_rules("mode.debug", "mode.release")

set_languages("c++23", "c17")

add_requires("websocketpp");
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

if is_plat("linux") then
    add_cxxflags("-pthread")
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
        add_links("pthread")
    end
