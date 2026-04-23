file(GLOB_RECURSE MODULE_SOURCES "*.c")
set (SOURCES ${MODULE_SOURCES})
set (INCLUDES include)
idf_component_register(
        SRCS ${SOURCES}
        INCLUDE_DIRS ${INCLUDES}
        REQUIRES driver tasks_scheduler shared_values m10 ble led_strip
)