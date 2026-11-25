cmake_minimum_required(VERSION 3.28)
project(toolchain_smoke CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(toolchain_smoke main.cpp)
target_compile_options(toolchain_smoke PRIVATE
    -Wall -Wextra -Wpedantic -Werror -fno-exceptions -fno-rtti)
target_link_libraries(toolchain_smoke PRIVATE deko3d)
nx_create_nro(toolchain_smoke)
