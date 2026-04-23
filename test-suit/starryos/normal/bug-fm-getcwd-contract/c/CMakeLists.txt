cmake_minimum_required(VERSION 3.20)

project(bug-fm-getcwd-contract C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(bug-fm-getcwd-contract src/main.c)
target_compile_options(bug-fm-getcwd-contract PRIVATE -Wall -Wextra -Werror)

install(TARGETS bug-fm-getcwd-contract RUNTIME DESTINATION usr/bin)
