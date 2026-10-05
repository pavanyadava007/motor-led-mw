# Portable middleware: plain C99, no Zephyr headers. In a Zephyr application it is
# compiled with the Zephyr target flags; any other CMake project can use it as is.
add_library(mlmw STATIC src/lin.c src/bldc.c src/led.c src/cmd.c)
target_include_directories(mlmw PUBLIC include)
if(TARGET zephyr_interface)
  target_link_libraries(mlmw PRIVATE zephyr_interface)
endif()
