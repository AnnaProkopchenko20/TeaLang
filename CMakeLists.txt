cmake_minimum_required(VERSION 3.20)
project(compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find LLVM to resolve LLVM headers referenced in visitor.h / AST nodes
find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs core support)

# Collect source files under src/
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Exclude codegen.cpp and semantic_checker.cpp from compilation
list(FILTER SOURCES EXCLUDE REGEX ".*(codegen|semantic_checker)\\.cpp$")

add_executable(compiler ${SOURCES})

# Link LLVM and include search paths
target_link_libraries(compiler PRIVATE ${llvm_libs})

target_include_directories(compiler PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/nodes
        ${CMAKE_CURRENT_SOURCE_DIR}/src/states
)