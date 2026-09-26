cmake_minimum_required(VERSION 3.28)
project(db-engine C)

set(CMAKE_C_STANDARD 11)

include_directories(include src)

set(ENGINE_SOURCES
    src/core/page.c
    src/core/header.c
    src/core/schema.c
    src/core/table.c
    src/commands/init_db.c
    src/commands/init_table.c
    src/commands/insert.c
)

add_executable(db-engine-test tests/tests.c ${ENGINE_SOURCES})
add_executable(db-demo demo/demo.c ${ENGINE_SOURCES})
