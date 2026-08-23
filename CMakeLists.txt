cmake_minimum_required(VERSION 3.28)
project(db-engine C)

set(CMAKE_C_STANDARD 11)

add_executable(db-engine tests.c engine/engine.c)
