# P.A.S.S. — Performance Adaptive SIMD Substitution
# Клиент DynamoRIO. Сборка:
#   mkdir build && cd build
#   cmake .. -DDynamoRIO_DIR=/path/to/DynamoRIO/cmake && make -j
cmake_minimum_required(VERSION 3.7)
project(PASS C)

set(CMAKE_C_STANDARD 11)

if(NOT DEFINED DynamoRIO_DIR AND DEFINED ENV{DYNAMORIO_DIR})
    set(DynamoRIO_DIR $ENV{DYNAMORIO_DIR})
endif()
# Обход бага bundled DrMemoryFrameworkConfig.cmake в C-проектах
# (пустой CMAKE_CXX_SIZEOF_DATA_PTR ломает if): отключаем поиск DRMF.
set(DrMemoryFramework_DIR "/nonexistent")
find_package(DynamoRIO REQUIRED)

add_library(pass_client SHARED
    src/pass.c
    src/features.c
    src/cpuid_hook.c
    src/emu.c
    src/rewrite.c
    src/rewrite_vex.c
    src/rewrite_special.c
    src/trap.c
    )

configure_DynamoRIO_client(pass_client)
use_DynamoRIO_extension(pass_client drmgr)

target_include_directories(pass_client PRIVATE src)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # Базовый уровень x86-64 (SSE2): сам клиент обязан исполняться
    # на «старом» CPU без AVX/SSE4. Эмулятор (emu.c) — вовсе без SIMD.
    target_compile_options(pass_client PRIVATE
        -O2 -g -Wall -Wextra -Wno-unused-parameter
        -msse2 -mno-sse3 -mno-ssse3 -mno-sse4.1 -mno-sse4.2
        -mno-avx -mno-avx2 -mno-fma -mno-bmi -mno-bmi2 -mno-lzcnt
        -mno-movbe -mno-aes -mno-pclmul)
    set_source_files_properties(src/cpuid_hook.c PROPERTIES
        COMPILE_OPTIONS "-mno-sse;-mno-sse2;-mno-mmx")
    # plain-сигнатура — согласована с DynamoRIOConfig.cmake
    target_link_libraries(pass_client m)
endif()
