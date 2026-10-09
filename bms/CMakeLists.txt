# Portable BMS library: plain C99, no Zephyr headers (plain add_library, as mlmw/).
add_library(bms STATIC src/bms.c src/bms_balance.c src/bms_can.c src/bms_e2e.c
            src/bms_ecm_lg_hg2.c src/bms_monitor.c src/bms_sm.c src/bms_soc.c)
target_include_directories(bms PUBLIC include)
target_compile_options(bms PRIVATE -ffp-contract=off)
if(TARGET zephyr_interface)
  target_link_libraries(bms PRIVATE zephyr_interface)
endif()
