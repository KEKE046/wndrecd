cmake_minimum_required(VERSION 3.5)
project(wndrecd VERSION 0.0.0)

add_subdirectory(simppl EXCLUDE_FROM_ALL)

add_executable(wndrecd main.cpp)
target_link_libraries(wndrecd PUBLIC simppl)
install(TARGETS wndrecd RUNTIME DESTINATION bin)