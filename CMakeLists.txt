cmake_minimum_required(VERSION 3.12)
project(tempsystem)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIR} include)

FILE(GLOB sources src/*.cpp)
add_executable(tempsystem ${sources})

target_link_libraries(tempsystem ${CURL_LIBRARIES} jsoncpp)