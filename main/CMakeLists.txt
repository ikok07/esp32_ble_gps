file(GLOB_RECURSE MODULE_SOURCES "*.c")
set (SOURCES ${MODULE_SOURCES})
set (INCLUDES include)

idf_component_register(
        SRCS ${SOURCES}
        INCLUDE_DIRS ${INCLUDES}
)

add_compile_definitions(
        DEBUG_MODE_ENABLED
)