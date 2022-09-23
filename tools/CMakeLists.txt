cmake_minimum_required(VERSION 3.10)
project(PageColoring)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

include_directories(/usr/local/include)

add_executable(colorctl colorctl.cpp)
add_executable(get_color get_color.cpp)
add_executable(reserve_color reserve_color.cpp)
