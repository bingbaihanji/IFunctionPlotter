cmake_minimum_required(VERSION 3.20)
project(IFunctionPlotter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/utf-8 /MP)
endif()

add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_win32.cpp
    third_party/imgui/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC
    third_party/imgui
    third_party/imgui/backends
)
target_compile_definitions(imgui PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

add_library(implot STATIC
    third_party/implot/implot.cpp
    third_party/implot/implot_items.cpp
)
target_include_directories(implot PUBLIC third_party/implot)
target_link_libraries(implot PUBLIC imgui)

add_executable(IFunctionPlotter WIN32
    src/main.cpp
    src/platform/App.cpp
    src/platform/DesktopCapture.cpp
    src/render/GlassRenderer.cpp
    src/core/Expression.cpp
    src/core/Parser.cpp
    src/core/Sampler.cpp
    src/app/PlotterApp.cpp
    src/ui/MainWindow.cpp
    src/ui/FunctionPanel.cpp
    src/ui/PlotPanel.cpp
    src/ui/UiWidgets.cpp
)
target_include_directories(IFunctionPlotter PRIVATE src)
target_link_libraries(IFunctionPlotter PRIVATE
    imgui
    implot
    d3d11
    dxgi
    d3dcompiler
    dwmapi
)
set_property(TARGET IFunctionPlotter PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
