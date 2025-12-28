cmake_minimum_required(VERSION 4.0)
project(HiveMind)

set(CMAKE_CXX_STANDARD 14)

add_executable(HiveMind
        HiveMind.cpp
        harta.h
        agenti.h
        settings.h)
