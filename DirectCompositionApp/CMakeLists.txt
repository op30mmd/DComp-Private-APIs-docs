cmake_minimum_required(VERSION 3.20)
project(DirectCompositionApp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# tree-sitter core (no WASM)
file(GLOB TS_CORE_SOURCES "${CMAKE_SOURCE_DIR}/../tree-sitter/lib/src/*.c")
list(FILTER TS_CORE_SOURCES EXCLUDE REGEX "lib\\.c$")

add_library(tree-sitter-core STATIC ${TS_CORE_SOURCES})
target_include_directories(tree-sitter-core PRIVATE
    "${CMAKE_SOURCE_DIR}/../tree-sitter/lib/src"
    "${CMAKE_SOURCE_DIR}/../tree-sitter/lib/src/wasm"
)
target_include_directories(tree-sitter-core PUBLIC
    "${CMAKE_SOURCE_DIR}/../tree-sitter/lib/include"
)
target_compile_definitions(tree-sitter-core PRIVATE TREE_SITTER_REUSE_ALLOCATOR)
if(MSVC)
    target_compile_options(tree-sitter-core PRIVATE /wd4018 /wd4232 /wd4244 /wd4267 /wd4701)
endif()

# tree-sitter-cpp grammar
add_library(tree-sitter-cpp STATIC
    "${CMAKE_SOURCE_DIR}/../tree-sitter-cpp/src/parser.c"
    "${CMAKE_SOURCE_DIR}/../tree-sitter-cpp/src/scanner.c"
)
target_include_directories(tree-sitter-cpp PRIVATE
    "${CMAKE_SOURCE_DIR}/../tree-sitter/lib/src"
    "${CMAKE_SOURCE_DIR}/../tree-sitter-cpp/src"
)
target_include_directories(tree-sitter-cpp PUBLIC
    "${CMAKE_SOURCE_DIR}/../tree-sitter-cpp/bindings/c"
    "${CMAKE_SOURCE_DIR}/../tree-sitter/lib/include"
)

# Main application
add_executable(DirectCompositionApp WIN32
    main.cpp
    DCompHelper.cpp
    DCompHelper.h
    DirectManipHelper.cpp
    DirectManipHelper.h
    TreeSitterHighlighter.cpp
    TreeSitterHighlighter.h
    PrivateApiDemo.cpp
    PrivateApiDemo.h
    PresentationFactoryDemo.cpp
    PresentationFactoryDemo.h
)

target_link_libraries(DirectCompositionApp PRIVATE
    tree-sitter-core
    tree-sitter-cpp
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
