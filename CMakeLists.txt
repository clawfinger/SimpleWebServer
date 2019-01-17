cmake_minimum_required(VERSION 2.8)
project(WebServer)
find_package(Boost 1.65.1)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    add_executable(${PROJECT_NAME} "main.cpp")
    target_link_libraries(${PROJECT_NAME} boost_system)
endif()
