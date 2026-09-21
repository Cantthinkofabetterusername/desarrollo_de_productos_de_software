cmake_minimum_required(VERSION 4.3)

project(Shrinkigon)

set(CMAKE_CXX_STANDARD 20)

add_executable(Shrinkigon
        main.cpp

        external/imgui/imgui.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp

        external/imgui/backends/imgui_impl_glfw.cpp
        imgui_opengl21_renderer.h
        imgui_opengl21_renderer.cpp
)

if(APPLE)
    set_target_properties(Shrinkigon PROPERTIES
            MACOSX_BUNDLE TRUE
    )
endif()

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED PATHS /opt/homebrew/opt/glfw)
find_package(Freetype REQUIRED)

target_include_directories(Shrinkigon PRIVATE
        external/imgui
        external/imgui/backends
)

target_link_libraries(Shrinkigon
        OpenGL::GL
        glfw
        Freetype::Freetype
)