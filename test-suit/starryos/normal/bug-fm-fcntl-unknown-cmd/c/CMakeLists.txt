cmake_minimum_required(VERSION 3.20)

project(bug-fm-fcntl-unknown-cmd C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(bug-fm-fcntl-unknown-cmd src/main.c)
target_compile_options(bug-fm-fcntl-unknown-cmd PRIVATE -Wall -Wextra -Werror)

install(TARGETS bug-fm-fcntl-unknown-cmd RUNTIME DESTINATION usr/bin)
