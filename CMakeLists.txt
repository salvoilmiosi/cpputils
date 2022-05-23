cmake_minimum_required(VERSION 3.6.0)
project(cpputils VERSION 0.1.0)

set(cpputils_srcs src/base64.cpp)

if(WIN32)
    list(APPEND cpputils_srcs src/ansicvt.cpp)
endif()

add_library(cpputils STATIC ${cpputils_srcs})
target_include_directories(cpputils PUBLIC include)

find_package(fmt REQUIRED)
if (TARGET fmt::fmt-header-only)
    target_link_libraries(cpputils PUBLIC fmt::fmt-header-only)
else()
    target_link_libraries(cpputils fmt::fmt)
endif()

add_subdirectory(external/boostpp)
target_link_libraries(cpputils PUBLIC boost_preprocessor)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE external/asio/asio/include)
if(MINGW)
    target_link_libraries(asio INTERFACE wsock32 ws2_32)
endif()