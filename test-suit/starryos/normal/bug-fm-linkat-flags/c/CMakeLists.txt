cmake_minimum_required(VERSION 3.20)

project(bug-fm-linkat-flags C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(bug-fm-linkat-flags src/main.c)
target_compile_options(bug-fm-linkat-flags PRIVATE -Wall -Wextra -Werror)

install(TARGETS bug-fm-linkat-flags RUNTIME DESTINATION usr/bin)
