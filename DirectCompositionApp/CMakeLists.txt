cmake_minimum_required(VERSION 3.20)
project(DirectCompositionApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(DirectCompositionApp WIN32
    main.cpp
    DCompHelper.cpp
    DCompHelper.h
    PrivateApiDemo.cpp
    PrivateApiDemo.h
    PresentationFactoryDemo.cpp
    PresentationFactoryDemo.h
)

target_link_libraries(DirectCompositionApp PRIVATE
    d3d11
    dxgi
    dcomp
    d2d1
    ole32
    uuid
)

target_compile_definitions(DirectCompositionApp PRIVATE
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    NOMINMAX
)

set_target_properties(DirectCompositionApp PROPERTIES
    VS_DPI_AWARE "ON"
)
