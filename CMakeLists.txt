cmake_minimum_required(VERSION 3.16)
project(Embedded_prj C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(INC_DIR ${CMAKE_SOURCE_DIR}/include)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  "${SRC_DIR}/*.c"
)

if(NOT SRC_FILES)
  message(WARNING "No C sources found under ${SRC_DIR}. Adjust file locations or the glob pattern.")
endif()

include_directories(${INC_DIR})

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# Default flags (kept in a project-local variable)
set(DEFAULT_C_FLAGS "-Wall -Wextra -Wno-unused-parameter -ffunction-sections -fdata-sections")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(DEFAULT_C_FLAGS "${DEFAULT_C_FLAGS} -Og -g")
else()
  set(DEFAULT_C_FLAGS "${DEFAULT_C_FLAGS} -O2")
endif()

# Combine user-provided CMAKE_C_FLAGS with defaults (if any)
if(DEFINED CMAKE_C_FLAGS AND CMAKE_C_FLAGS)
  set(PROJECT_C_FLAGS "${CMAKE_C_FLAGS} ${DEFAULT_C_FLAGS}")
else()
  set(PROJECT_C_FLAGS "${DEFAULT_C_FLAGS}")
endif()

# Convert space-separated string into a CMake list so each flag becomes a separate item
string(REGEX REPLACE "^ +| +$" "" PROJECT_C_FLAGS "${PROJECT_C_FLAGS}")  # trim
string(REGEX REPLACE " +" ";" PROJECT_C_FLAGS_LIST "${PROJECT_C_FLAGS}")

set(LINKER_SCRIPT "" CACHE STRING "Path to a linker script (e.g. flash.ld)")
if(LINKER_SCRIPT)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${LINKER_SCRIPT} -Wl,--gc-sections")
endif()

set(OBJCOPY_TOOL "" CACHE STRING "Override objcopy binary (e.g. arm-none-eabi-objcopy)")
set(SIZE_TOOL "" CACHE STRING "Override size binary (e.g. arm-none-eabi-size)")

add_executable(${PROJECT_NAME}.elf ${SRC_FILES})

# Apply flags as a list so each flag is passed correctly
target_compile_options(${PROJECT_NAME}.elf PRIVATE ${PROJECT_C_FLAGS_LIST})
target_include_directories(${PROJECT_NAME}.elf PRIVATE ${INC_DIR})

# --- Find and link cJSON and libwebsockets via pkg-config ---
find_package(PkgConfig QUIET)

if(PKG_CONFIG_FOUND)
  pkg_check_modules(CJSON QUIET libcjson)
  pkg_check_modules(LWS QUIET libwebsockets)
endif()

# If pkg-config found the modules, add include dirs and libraries
if(CJSON_FOUND)
  target_include_directories(${PROJECT_NAME}.elf PRIVATE ${CJSON_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME}.elf PRIVATE ${CJSON_LIBRARIES})
  link_directories(${CJSON_LIBRARY_DIRS})
  message(STATUS "Found cJSON via pkg-config: ${CJSON_VERSION}")
endif()

if(LWS_FOUND)
  target_include_directories(${PROJECT_NAME}.elf PRIVATE ${LWS_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME}.elf PRIVATE ${LWS_LIBRARIES})
  link_directories(${LWS_LIBRARY_DIRS})
  message(STATUS "Found libwebsockets via pkg-config: ${LWS_VERSION}")
endif()

# Simple fallback: if pkg-config not available or modules not found, try typical names
if(NOT CJSON_FOUND)
  find_path(CJSON_INCLUDE_PATH
    NAMES cJSON.h
    PATH_SUFFIXES cjson
    )   
  find_library(CJSON_LIB NAMES cjson)
  if(CJSON_INCLUDE_PATH AND CJSON_LIB)
    target_include_directories(${PROJECT_NAME}.elf PRIVATE ${CJSON_INCLUDE_PATH})
    target_link_libraries(${PROJECT_NAME}.elf PRIVATE ${CJSON_LIB})
    message(STATUS "Found cJSON via find_package/find_library")
  else()
    message(WARNING "cJSON not found. You will get undefined references to cJSON_* unless you install and expose cJSON development files.")
  endif()
endif()

if(NOT LWS_FOUND)
  find_path(LWS_INCLUDE_PATH libwebsockets.h)
  find_library(LWS_LIB NAMES websockets websockets_shared libwebsockets)
  if(LWS_INCLUDE_PATH AND LWS_LIB)
    target_include_directories(${PROJECT_NAME}.elf PRIVATE ${LWS_INCLUDE_PATH})
    target_link_libraries(${PROJECT_NAME}.elf PRIVATE ${LWS_LIB})
    message(STATUS "Found libwebsockets via find_package/find_library")
  else()
    message(WARNING "libwebsockets not found. You will get undefined references to lws_* unless you install and expose libwebsockets development files.")
  endif()
endif()

# link the dynamic loader library if needed (dl on Linux)
if(UNIX)
  target_link_libraries(${PROJECT_NAME}.elf PRIVATE ${CMAKE_DL_LIBS})
endif()

# Objcopy -> .bin generation (unchanged)
if(OBJCOPY_TOOL)
  find_program(OBJCOPY ${OBJCOPY_TOOL})
else()
  find_program(OBJCOPY arm-none-eabi-objcopy)
  if(NOT OBJCOPY)
    find_program(OBJCOPY objcopy)
  endif()
endif()

if(OBJCOPY)
  set(BIN_OUTPUT "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin")
  add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_OUTPUT}
    COMMENT "Generating binary ${BIN_OUTPUT}"
  )
endif()

# Size target
if(SIZE_TOOL)
  find_program(SIZE_EXEC ${SIZE_TOOL})
else()
  find_program(SIZE_EXEC arm-none-eabi-size)
  if(NOT SIZE_EXEC)
    find_program(SIZE_EXEC size)
  endif()
endif()

if(SIZE_EXEC)
  add_custom_target(size
    COMMAND ${SIZE_EXEC} $<TARGET_FILE:${PROJECT_NAME}.elf>
    DEPENDS ${PROJECT_NAME}.elf
    COMMENT "Print size of ELF"
  )
endif()

add_custom_target(print-config
  COMMAND ${CMAKE_COMMAND} -E echo "Project: ${PROJECT_NAME}"
  COMMAND ${CMAKE_COMMAND} -E echo "Build type: ${CMAKE_BUILD_TYPE}"
  COMMAND ${CMAKE_COMMAND} -E echo "Src dir: ${SRC_DIR}"
  COMMAND ${CMAKE_COMMAND} -E echo "Inc dir: ${INC_DIR}"
  COMMENT "Print basic project configuration"
)

# Notes:
# - For cross-compilation use -DCMAKE_TOOLCHAIN_FILE=...
# - To set a linker script: -DLINKER_SCRIPT=path/to/flash.ld
# - To override objcopy/size: -DOBJCOPY_TOOL=... -DSIZE_TOOL=...