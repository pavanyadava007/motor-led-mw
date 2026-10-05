# Generic on-target test harness: builds the middleware (MLMW_DIR, so mutants can be
# swapped in) with every *.c in TEST_SRC_DIR as one ztest image.
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mlmw_tests)

if(NOT DEFINED MLMW_DIR)
  set(MLMW_DIR ${CMAKE_CURRENT_LIST_DIR}/../../mlmw)
endif()
if(NOT DEFINED TEST_SRC_DIR)
  set(TEST_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../ref/src)
endif()

add_subdirectory(${MLMW_DIR} mlmw)
file(GLOB TEST_SOURCES ${TEST_SRC_DIR}/*.c)
target_sources(app PRIVATE ${TEST_SOURCES})
target_link_libraries(app PRIVATE mlmw)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
