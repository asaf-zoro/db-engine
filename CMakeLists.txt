cmake_minimum_required(VERSION 3.28)
project(db-engine C)

set(CMAKE_C_STANDARD 11)

include_directories(include src)

add_executable(db-engine
    tests/main.c
    src/core/page.c
    src/core/header.c
    src/commands/init_db.c
    src/commands/init_table.c
)
