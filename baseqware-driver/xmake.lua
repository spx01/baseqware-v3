add_rules("mode.debug", "mode.release")

set_languages("c++23", "c17")

if is_plat("windows") then
    add_cxxflags("/EHsc")
end

target("userland")
    set_kind("binary")
    add_links("Advapi32")
    add_files("exe/*.cpp")
    add_files("exe/install.c")

target("SIoctl")
    add_rules("wdk.driver", "wdk.env.wdm")
    set_values("wdk.sign.mode", "test")
    add_values("wdk.man.flags", "-prefix Kcs")
    add_values("wdk.man.resource", "kcsCounters.rc")
    add_values("wdk.man.header", "kcsCounters.h")
    add_values("wdk.man.counter_header", "kcsCounters_counters.h")
    add_files("sys/*.c", "sys/*.rc")
    add_values("wdk.sign.digest_algorithm", "SHA256")
    set_values("wdk.sign.thumbprint", "d29954da23035b3e92dc34b7863ecf44993ae788")
    set_values("wdk.sign.store", "PrivateCertStore")
    set_values("wdk.sign.company", "tboox.org(test)")

