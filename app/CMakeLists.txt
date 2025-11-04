cmake_minimum_required(VERSION 3.20)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(z_lora)

target_sources(app PRIVATE src/main.c)
