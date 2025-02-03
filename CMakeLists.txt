cmake_minimum_required(VERSION 3.29)
project(Slag_DearIMGui_backend)

set(CMAKE_CXX_STANDARD 20)

#get slag library
include(FetchContent)
fetchcontent_declare(
        Slag
        GIT_REPOSITORY https://github.com/Joshua-A-Shelton/Slag.git
        GIT_TAG origin/master
        GIT_SHALLOW TRUE
        OVERRIDE_FIND_PACKAGE
)
fetchcontent_makeavailable(Slag)

find_package(Slag REQUIRED)


add_library(SlagDearIMGuiBackend
        imgui_impl_slag.cpp
        imgui_impl_slag.h)

target_link_libraries(SlagDearIMGuiBackend Slag)

OPTION(BUILD_SLAG_IMGUI_BACKEND_EXAMPLE "Build Slag imgui example" ON)

if(${BUILD_SLAG_IMGUI_BACKEND_EXAMPLE})

    find_package(SDL2 REQUIRED)
    if(WIN32)
        OPTION(SLAG_WINDOWS_BACKEND "Use Win32 Windowing Backend" ON)
    else ()
        OPTION(SLAG_X11_BACKEND "Use X11 Windowing Backend" ON)
    endif()

    add_executable(SlagDearIMGuiBackendExample
            example/main.cpp
            example/imgui_impl_sdl2.cpp
            example/imgui_impl_sdl2.h
            example/imgui.cpp
            example/imgui.h
            example/imconfig.h
            example/imgui_demo.cpp
            example/imgui_draw.cpp
            example/imgui_internal.h
            example/imgui_tables.cpp
            example/imgui_widgets.cpp
            example/imstb_rectpack.h
            example/imstb_textedit.h
            example/imstb_truetype.h
    )
    target_link_libraries(SlagDearIMGuiBackendExample SDL2::SDL2 SlagDearIMGuiBackend)
    target_include_directories(SlagDearIMGuiBackendExample PRIVATE ${SDL2_LIBRARIES})
    target_include_directories(SlagDearIMGuiBackend PUBLIC example)
endif()