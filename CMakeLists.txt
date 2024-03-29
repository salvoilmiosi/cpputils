cmake_minimum_required(VERSION 3.6.0)
project(cpputils VERSION 0.1.0)

add_library(cpputils INTERFACE)
target_include_directories(cpputils INTERFACE include)

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

add_subdirectory(external/boostpp)
target_link_libraries(cpputils INTERFACE boost_preprocessor)

add_subdirectory(external/json)
target_link_libraries(cpputils INTERFACE nlohmann_json::nlohmann_json)