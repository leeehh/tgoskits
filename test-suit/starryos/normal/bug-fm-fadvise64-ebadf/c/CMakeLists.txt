cmake_minimum_required(VERSION 3.20)

project(bug-fm-fadvise64-ebadf C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(bug-fm-fadvise64-ebadf src/main.c)
target_compile_options(bug-fm-fadvise64-ebadf PRIVATE -Wall -Wextra -Werror)

install(TARGETS bug-fm-fadvise64-ebadf RUNTIME DESTINATION usr/bin)
