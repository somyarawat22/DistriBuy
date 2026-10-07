cmake_minimum_required(VERSION 3.16)

project(ecommerce_coordinator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PostgreSQL REQUIRED)

add_executable(coordinator src/main.cpp)
target_include_directories(coordinator PRIVATE ${PostgreSQL_INCLUDE_DIRS})
target_link_libraries(coordinator PRIVATE ${PostgreSQL_LIBRARIES})