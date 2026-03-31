# Sample CMake file demonstrating syntax highlighting features
# This file exercises all TS captures from cmake-highlights.scm

cmake_minimum_required(VERSION 3.16)
project(SampleProject VERSION 1.0.0 LANGUAGES C CXX)

# Control flow: if/elseif/else/endif -> @function.special
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Building in release mode")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Building in debug mode")
else()
    message(WARNING "Unknown build type")
endif()

# Variable references ${...} -> @variable.special
set(MY_SOURCES main.c util.c helper.c)
set(MY_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include)
message(STATUS "Sources: ${MY_SOURCES}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

# Foreach loop -> @function.special
foreach(SRC ${MY_SOURCES})
    message(STATUS "Source file: ${SRC}")
endforeach()

# While loop -> @function.special
set(COUNTER 0)
while(COUNTER LESS 5)
    math(EXPR COUNTER "${COUNTER} + 1")
endwhile()

# Function definition -> @function.special
function(my_custom_function TARGET_NAME)
    target_compile_definitions(${TARGET_NAME} PRIVATE DEBUG_MODE=1)
    target_include_directories(${TARGET_NAME} PUBLIC ${MY_INCLUDES})
endfunction()

# Macro definition -> @function.special
macro(setup_warnings TARGET)
    target_compile_options(${TARGET} PRIVATE -Wall -Wextra)
endmacro()

# Block -> @function.special
block(SCOPE_FOR VARIABLES)
    set(TEMP_VAR "inside block")
    message(STATUS "Block var: ${TEMP_VAR}")
endblock()

# Normal commands -> @function.special (identifier)
find_package(Threads REQUIRED)
find_library(MATH_LIB m)

# Library and executable targets
add_library(mylib STATIC ${MY_SOURCES})
add_executable(myapp main.c)
target_link_libraries(myapp PRIVATE mylib Threads::Threads)

# Properties/constants (ALL_CAPS) -> @tag.special
set_target_properties(mylib PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "include/mylib.h"
    POSITION_INDEPENDENT_CODE ON
)

# Install rules
install(TARGETS myapp mylib
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

# Strings -> @string (quoted_argument)
set(GREETING "Hello from CMake")
string(TOUPPER "${GREETING}" UPPER_GREETING)
string(REGEX REPLACE "[aeiou]" "_" MODIFIED "${GREETING}")

# Unquoted arguments -> @variable
option(ENABLE_TESTING "Enable unit tests" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Nested variable references
set(CONFIG_${CMAKE_BUILD_TYPE}_FLAGS "-O2 -DNDEBUG")
message(STATUS "Flags: ${CONFIG_${CMAKE_BUILD_TYPE}_FLAGS}")

# Bracket comment -> @comment
#[[
  This is a bracket comment.
  It can span multiple lines.
  Used for longer documentation blocks.
]]

# Various commands exercising highlighting
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
configure_file(config.h.in config.h @ONLY)
export(TARGETS mylib FILE MyLibTargets.cmake)
enable_testing()
add_test(NAME basic_test COMMAND myapp --test)

# Generator expressions with variable refs
target_compile_definitions(myapp PRIVATE
    $<$<CONFIG:Debug>:DEBUG_BUILD>
    $<$<CONFIG:Release>:NDEBUG>
)

# CPack configuration
set(CPACK_PACKAGE_NAME "SampleProject")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "TGZ;DEB")
include(CPack)
