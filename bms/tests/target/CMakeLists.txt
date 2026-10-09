# The portable BMS unit tests (bms/tests/unit) as one ztest image for mps2/an385
# (Cortex-M3, no FPU: float32 runs in soft float). BMS_DIR can point at a mutant copy.
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bms_tests)

if(NOT DEFINED BMS_DIR)
  set(BMS_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
endif()
add_subdirectory(${BMS_DIR} bms)
file(GLOB TEST_SOURCES ${CMAKE_CURRENT_LIST_DIR}/../unit/*.c)
target_sources(app PRIVATE ${TEST_SOURCES})
target_link_libraries(app PRIVATE bms)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../unit)
target_compile_options(app PRIVATE -ffp-contract=off)
