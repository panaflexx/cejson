# CMakeLists.txt – cejson (header-only) + test suite + fuzzer
cmake_minimum_required(VERSION 3.14)
project(cejson LANGUAGES C)

# ------------------------------------------------------------------
# Global compiler flags (same as your manual command)
# ------------------------------------------------------------------
add_compile_options(
    -Wall -Wextra -Wpedantic
    -g3 -O3
    -fsanitize=address,undefined
    #-fno-omit-frame-pointer
)

add_link_options(
    -fsanitize=address,undefined
)

# ------------------------------------------------------------------
# Include directory so #include "cejson.h" works
# ------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# ------------------------------------------------------------------
# Executables – all header-only, no library needed
# ------------------------------------------------------------------

# 1. Test suite
add_executable(cejson-test-suite cejson-test-suite.c)
set_target_properties(cejson-test-suite PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

# 2. Fuzzer (with full sanitizers – perfect for development & hunting bugs)
add_executable(cejson-fuzz cejson-fuzz.c)
set_target_properties(cejson-fuzz PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

# 3. Optional: ultra-fast release fuzzer (no sanitizers, max performance)
add_executable(cejson-fuzz-fast cejson-fuzz.c)
target_compile_options(cejson-fuzz-fast PRIVATE
    -O3 -march=native -flto -DNDEBUG
)
target_link_options(cejson-fuzz-fast PRIVATE -flto)
set_target_properties(cejson-fuzz-fast PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
    OUTPUT_NAME cejson-fuzz-fast
)

# 4. Optional: file tester (cejson-files.c)
add_executable(cejson-files cejson-files.c)
set_target_properties(cejson-files PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)
