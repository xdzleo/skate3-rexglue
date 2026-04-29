
# skate3 - ReXGlue Recompiled Project
#
# This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(skate3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(SKATE3_SOURCES
    src/main.cpp
    src/hooks.cpp
    src/vp6_bridge.cpp
)

if(WIN32)
    add_executable(skate3 WIN32 ${SKATE3_SOURCES})
else()
    add_executable(skate3 ${SKATE3_SOURCES})
endif()

rexglue_setup_target(skate3)

# Link libavcodec so MoviePlayer2 hook can decode Skate 3's VP6 intro
# videos (otherwise the title would loop forever in the intro state with
# no decoded frames). libavcodec already exists as a static lib in the
# rex SDK third-party tree (linked PRIVATE to rex::audio for WMA Pro);
# we add it directly here so VP6 codec entry points are reachable from
# our hooks.
target_link_libraries(skate3 PRIVATE libavcodec libavutil)
