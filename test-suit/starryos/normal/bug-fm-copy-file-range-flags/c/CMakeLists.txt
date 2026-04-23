cmake_minimum_required(VERSION 3.20)

project(bug-fm-copy-file-range-flags C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(bug-fm-copy-file-range-flags src/main.c)
target_compile_options(bug-fm-copy-file-range-flags PRIVATE -Wall -Wextra -Werror)

install(TARGETS bug-fm-copy-file-range-flags RUNTIME DESTINATION usr/bin)
