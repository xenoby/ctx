cmake_minimum_required(VERSION 3.19)

if(DEFINED ENV{PROJECT_CUSTOM_NAME})
    set(CHOSEN_PROJECT_NAME "$ENV{PROJECT_CUSTOM_NAME}")
else()
    set(CHOSEN_PROJECT_NAME "ctx")
endif()

project(${CHOSEN_PROJECT_NAME} VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

file(GLOB_RECURSE SRC_FILES
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/*.c"
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

if(NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE -Werror -g)
    endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(NOT MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE -O3)
        target_link_options(${PROJECT_NAME} PRIVATE -static-libgcc -static-libstdc++ -static)
    endif()
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PROJECT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_SOURCE_DIR}/bin"
)
