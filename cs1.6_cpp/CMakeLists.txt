cmake_minimum_required(VERSION 3.16)
project(CS16AHK_CPP LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/W4 /EHsc)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(CS16AHK_CPP WIN32
    src/main.cpp
    src/resource.rc
)

target_compile_definitions(CS16AHK_CPP PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

target_link_libraries(CS16AHK_CPP PRIVATE user32 gdi32 winmm advapi32 psapi avrt)
