cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(motor_led_node)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../mlmw mlmw)
target_sources(app PRIVATE src/main.c src/plant.c src/net.c src/mac_psa.c)
target_link_libraries(app PRIVATE mlmw)
