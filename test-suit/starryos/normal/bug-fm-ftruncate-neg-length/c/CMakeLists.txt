cmake_minimum_required(VERSION 3.20)

project(bug-fm-ftruncate-neg-length C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(bug-fm-ftruncate-neg-length src/main.c)
target_compile_options(bug-fm-ftruncate-neg-length PRIVATE -Wall -Wextra -Werror)

install(TARGETS bug-fm-ftruncate-neg-length RUNTIME DESTINATION usr/bin)
