cmake_minimum_required(VERSION 3.20)

project(Shrinkigon)

set(CMAKE_CXX_STANDARD 20)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
endif()

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED PATHS /opt/homebrew/opt/glfw)
find_package(Freetype REQUIRED)
find_package(assimp REQUIRED)

add_executable(Shrinkigon
        main.cpp

        external/imgui/imgui.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp

        external/imgui/backends/imgui_impl_glfw.cpp
        imgui_opengl21_renderer.h
        imgui_opengl21_renderer.cpp
        stb_image.h
)

if(APPLE)
    set_target_properties(Shrinkigon PROPERTIES
            MACOSX_BUNDLE TRUE
    )
endif()

target_include_directories(Shrinkigon PRIVATE
        external/imgui
        external/imgui/backends
        /opt/homebrew/include
)

target_link_libraries(Shrinkigon PRIVATE
        OpenGL::GL
        glfw
        Freetype::Freetype
        assimp::assimp
)