cmake_minimum_required(VERSION 3.10)
project(lib_operator C)

set(CMAKE_C_STANDARD 11)

# Library
add_library(operator_lib STATIC
    src/lib_operator.c
)

target_include_directories(operator_lib PUBLIC
    inc
)

# Sample executable to test the library
add_executable(operator_example
    src/example.c
)

target_link_libraries(operator_example PRIVATE
    operator_lib
)
