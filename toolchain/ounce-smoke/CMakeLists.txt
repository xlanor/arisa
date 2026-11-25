cmake_minimum_required(VERSION 3.28)
project(toolchain_ounce_smoke CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT NINTENDO_SWITCH2)
    message(FATAL_ERROR "the ounce smoke needs libounce's Switch2.cmake")
endif()

add_executable(toolchain_ounce_smoke main.cpp)
target_compile_options(toolchain_ounce_smoke PRIVATE
    -Wall -Wextra -Wpedantic -Werror -fno-exceptions -fno-rtti)
target_link_libraries(toolchain_ounce_smoke PRIVATE vulkan)
nx_create_nro(toolchain_ounce_smoke)
