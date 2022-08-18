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

add_subdirectory(external/boostpp)
target_link_libraries(cpputils INTERFACE boost_preprocessor)

add_subdirectory(external/jsoncpp)
target_link_libraries(cpputils INTERFACE jsoncpp_static)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE external/asio/asio/include)
if(MINGW)
    target_link_libraries(asio INTERFACE wsock32 ws2_32)
endif()