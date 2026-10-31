cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dsp_lab_bench)

# the filters and the portable benchmark core are shared with the PC build; only src/main.c is specific to the board
target_sources(app PRIVATE
    src/main.c
    ${CMAKE_CURRENT_LIST_DIR}/../core/bench_core.c
    ${CMAKE_CURRENT_LIST_DIR}/../../c/dsp.c)
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../core
    ${CMAKE_CURRENT_LIST_DIR}/../../c)
