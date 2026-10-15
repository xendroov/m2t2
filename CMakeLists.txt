cmake_minimum_required(VERSION 3.16)
project(m2t2bot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows DLL hedefi — x86 (32-bit) Metin2 client için
# CMake GUI'da: -A Win32  veya  -DCMAKE_GENERATOR_PLATFORM=Win32

add_library(m2t2bot SHARED
    src/dllmain.cpp
    src/core/console.cpp
    src/core/hooks.cpp
    src/core/memory.cpp
    src/core/sigscan.cpp
    src/core/d3dhook.cpp
    src/core/wndhook.cpp
    src/game/pyapi.cpp
    src/game/entity.cpp
    src/game/player.cpp
    src/network/packet_logger.cpp
    src/network/send_hook.cpp
    src/bot/bot.cpp
    src/bot/states/farm_state.cpp
    src/bot/states/loot_state.cpp
)

target_include_directories(m2t2bot PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(m2t2bot PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

# python27.dll runtime'da GetProcAddress ile yukleniyor.
# Python SDK veya .lib dosyasina GEREK YOK.

# Metin2 32-bit process olduğu için DLL de 32-bit olmalı
if(MSVC)
    target_compile_options(m2t2bot PRIVATE /W3 /O2)
endif()
