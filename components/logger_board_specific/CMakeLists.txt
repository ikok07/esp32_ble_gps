file(GLOB_RECURSE MODULE_SOURCES "*.c")
set (SOURCES ${MODULE_SOURCES})
set (INCLUDES ../logger/include)
idf_component_register(
        SRCS ${SOURCES}
        INCLUDE_DIRS ${INCLUDES}
        REQUIRES driver
)