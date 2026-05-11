cmake_minimum_required(VERSION 3.20)
project(DistributedTetris LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 CONFIG REQUIRED)

# ENet naming differs across package managers; vcpkg provides unofficial-enet.
find_package(unofficial-enet CONFIG QUIET)
find_package(ENet CONFIG QUIET)
find_package(enet CONFIG QUIET)

find_package(Threads REQUIRED)

add_executable(distributed_tetris
    src/main.cpp
    src/core/App.cpp
    src/network/NetworkNode.cpp
    src/simulation/TetrisGame.cpp
)

target_include_directories(distributed_tetris PRIVATE
    include
)

target_link_libraries(distributed_tetris PRIVATE
    SDL2::SDL2
    $<$<TARGET_EXISTS:SDL2::SDL2main>:SDL2::SDL2main>
    $<$<TARGET_EXISTS:unofficial::enet::enet>:unofficial::enet::enet>
    $<$<TARGET_EXISTS:ENet::enet>:ENet::enet>
    $<$<TARGET_EXISTS:enet::enet>:enet::enet>
    $<$<TARGET_EXISTS:enet>:enet>
    Threads::Threads
)

if (WIN32)
    target_link_libraries(distributed_tetris PRIVATE ws2_32 winmm)
endif()

if (
    NOT TARGET unofficial::enet::enet
    AND NOT TARGET ENet::enet
    AND NOT TARGET enet::enet
    AND NOT TARGET enet
)
    message(FATAL_ERROR "No ENet CMake target found. For vcpkg install enet:x64-windows and pass -DCMAKE_TOOLCHAIN_FILE=... and -DVCPKG_TARGET_TRIPLET=x64-windows.")
endif()
