cmake_minimum_required(VERSION 3.5)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(anjay REQUIRED)

add_library(toyota_remote STATIC
            src/Main_Objects/firmware_update.c
            src/Main_Objects/humidity.c
            src/Main_Objects/headlights_control.c
            src/toyota_client.c
            src/toyota_utils.c)

target_include_directories(toyota_remote PUBLIC include PRIVATE include/Main_Objects)
target_compile_options(toyota_remote PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(toyota_remote PUBLIC anjay_static)



