cmake_minimum_required(VERSION 4.3)

project(Shrinkigon)

set(CMAKE_CXX_STANDARD 20)

add_executable(Shrinkigon
        main.cpp
)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED PATHS /opt/homebrew/opt/glfw)

target_link_libraries(Shrinkigon PRIVATE
        OpenGL::GL
        glfw
)