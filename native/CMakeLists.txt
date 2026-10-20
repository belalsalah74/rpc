cmake_minimum_required(VERSION 3.22.1)
project(pokemon_go_rpc_probe C)

add_library(pgo_rpc_probe SHARED libpgo_rpc_probe.c)
set_target_properties(pgo_rpc_probe PROPERTIES OUTPUT_NAME pgo_rpc_probe)

add_library(main_wrapper SHARED libmain_wrapper.c)
set_target_properties(main_wrapper PROPERTIES OUTPUT_NAME main)
target_link_options(main_wrapper PRIVATE "-Wl,-soname,libmain_wrapper.so")

find_library(log-lib log)
find_library(dl-lib dl)
target_link_libraries(pgo_rpc_probe ${log-lib} ${dl-lib})
target_link_libraries(main_wrapper ${log-lib} ${dl-lib})
