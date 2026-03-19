cmake_minimum_required(VERSION 3.16)
project(linuxdospace_c_sdk C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(linuxdospace_c STATIC
  src/sdk.c
)

target_include_directories(linuxdospace_c PUBLIC include)

if(MSVC)
  target_compile_options(linuxdospace_c PRIVATE /W4)
else()
  target_compile_options(linuxdospace_c PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(linuxdospace_c_example
  examples/simple.c
)
target_link_libraries(linuxdospace_c_example PRIVATE linuxdospace_c)
