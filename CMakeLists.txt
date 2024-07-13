cmake_minimum_required(VERSION 3.6.0)
project(cpputils VERSION 0.1.0)

add_library(cpputils INTERFACE)
set_property(TARGET cpputils PROPERTY CXX_STANDARD 20)
set_property(TARGET cpputils PROPERTY CXX_STANDARD_REQUIRED ON)

target_include_directories(cpputils INTERFACE include external/reflect)

find_package(fmt REQUIRED)
if(TARGET fmt::fmt-header-only)
    target_link_libraries(cpputils INTERFACE fmt::fmt-header-only)
else()
    target_link_libraries(cpputils INTERFACE fmt::fmt)
endif()

if(MSVC)
    target_compile_definitions(cpputils INTERFACE _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
endif()

find_package(range-v3 REQUIRED)
target_link_libraries(cpputils INTERFACE range-v3)

add_subdirectory(external/json)
target_link_libraries(cpputils INTERFACE nlohmann_json::nlohmann_json)

target_compile_definitions(cpputils INTERFACE NTEST)
