cmake_minimum_required(VERSION 3.15)

add_executable(EmptyCPP Empty.cpp)
target_link_libraries(EmptyCPP RayTracedGL1)

add_executable(EmptyC Empty.c)
target_link_libraries(EmptyC RayTracedGL1)
