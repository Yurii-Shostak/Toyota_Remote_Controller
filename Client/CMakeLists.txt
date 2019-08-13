cmake_minimum_required(VERSION 3.5)

add_executable(toyota_remote_controller
    main.c
    file_parser.c)
target_compile_options(toyota_remote_controller PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(toyota_remote_controller PRIVATE toyota_remote)
