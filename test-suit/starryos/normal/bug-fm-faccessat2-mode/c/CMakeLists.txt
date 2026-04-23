cmake_minimum_required(VERSION 3.20)

project(bug-fm-faccessat2-mode C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(bug-fm-faccessat2-mode src/main.c)
target_compile_options(bug-fm-faccessat2-mode PRIVATE -Wall -Wextra -Werror)

install(TARGETS bug-fm-faccessat2-mode RUNTIME DESTINATION usr/bin)
