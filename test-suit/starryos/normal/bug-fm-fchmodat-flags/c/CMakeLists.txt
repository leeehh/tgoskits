cmake_minimum_required(VERSION 3.20)

project(bug-fm-fchmodat-flags C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(bug-fm-fchmodat-flags src/main.c)
target_compile_options(bug-fm-fchmodat-flags PRIVATE -Wall -Wextra -Werror)

install(TARGETS bug-fm-fchmodat-flags RUNTIME DESTINATION usr/bin)
