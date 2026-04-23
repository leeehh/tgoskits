cmake_minimum_required(VERSION 3.20)

project(bug-fm-signalfd4-cloexec-update C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(bug-fm-signalfd4-cloexec-update src/main.c)
target_compile_options(bug-fm-signalfd4-cloexec-update PRIVATE -Wall -Wextra -Werror)

install(TARGETS bug-fm-signalfd4-cloexec-update RUNTIME DESTINATION usr/bin)
