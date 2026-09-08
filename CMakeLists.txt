cmake_minimum_required(VERSION 3.16)

project(Embedded_prj C)

# ------------------------------------------------------------
# C standard
# ------------------------------------------------------------

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# We don't need position-independent code for the executable.
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

# ------------------------------------------------------------
# Build type
# ------------------------------------------------------------

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Build type"
        FORCE
    )
endif()

# ------------------------------------------------------------
# Project directories
# ------------------------------------------------------------

set(SRC_DIR
    "${CMAKE_SOURCE_DIR}/src"
)

set(INC_DIR
    "${CMAKE_SOURCE_DIR}/include"
)

# ------------------------------------------------------------
# Source files
# ------------------------------------------------------------

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    "${SRC_DIR}/*.c"
)

if(NOT SRC_FILES)
    message(FATAL_ERROR
        "No C source files found under ${SRC_DIR}"
    )
endif()

# ------------------------------------------------------------
# Executable
# ------------------------------------------------------------

add_executable(${PROJECT_NAME}
    ${SRC_FILES}
)

# ------------------------------------------------------------
# Project headers
# ------------------------------------------------------------

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${INC_DIR}"
)

# ------------------------------------------------------------
# Compiler warnings
# ------------------------------------------------------------

target_compile_options(${PROJECT_NAME}
    PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -ffunction-sections
        -fdata-sections
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        _POSIX_C_SOURCE=200809L
)
# ------------------------------------------------------------
# Optimization / Debug
# ------------------------------------------------------------

if(CMAKE_BUILD_TYPE STREQUAL "Debug")

    target_compile_options(${PROJECT_NAME}
        PRIVATE
            -Og
            -g
    )

else()

    target_compile_options(${PROJECT_NAME}
        PRIVATE
            -O2
    )

endif()

# ------------------------------------------------------------
# pkg-config
# ------------------------------------------------------------

find_package(PkgConfig REQUIRED)

# ------------------------------------------------------------
# cJSON
# ------------------------------------------------------------

pkg_check_modules(CJSON REQUIRED libcjson)

# ------------------------------------------------------------
# libwebsockets
# ------------------------------------------------------------

pkg_check_modules(LWS REQUIRED libwebsockets)

# ------------------------------------------------------------
# Third-party include directories
# ------------------------------------------------------------

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CJSON_INCLUDE_DIRS}
        ${LWS_INCLUDE_DIRS}
)

# ------------------------------------------------------------
# Third-party library directories
# ------------------------------------------------------------

target_link_directories(${PROJECT_NAME}
    PRIVATE
        ${CJSON_LIBRARY_DIRS}
        ${LWS_LIBRARY_DIRS}
)

# ------------------------------------------------------------
# Libraries
# ------------------------------------------------------------

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${CJSON_LIBRARIES}
        ${LWS_LIBRARIES}
        ${CMAKE_DL_LIBS}
)

# ------------------------------------------------------------
# Project information
# ------------------------------------------------------------

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Embedded-prj configuration")
message(STATUS "========================================")
message(STATUS "Project:       ${PROJECT_NAME}")
message(STATUS "System:        ${CMAKE_SYSTEM_NAME}")
message(STATUS "Architecture:  ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Compiler:      ${CMAKE_C_COMPILER}")
message(STATUS "Sysroot:       ${CMAKE_SYSROOT}")
message(STATUS "Build type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "CJSON libs:    ${CJSON_LIBRARIES}")
message(STATUS "LWS libs:      ${LWS_LIBRARIES}")
message(STATUS "========================================")
message(STATUS "")

# ------------------------------------------------------------
# Binary size target
# ------------------------------------------------------------

add_custom_target(size
    COMMAND
        ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
    DEPENDS
        ${PROJECT_NAME}
    COMMENT "Binary size:"
)