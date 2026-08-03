#!/usr/bin/env bash
# 프론트엔드(Qt) 정적분석.
set -e
BUILD=${1:-build}

# compile_commands.json 필요: CMake 옵션 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p "$BUILD" src/*.cpp

cppcheck --enable=warning,performance,portability \
         --std=c++17 --language=c++ --inline-suppr \
         --suppress=missingIncludeSystem \
         -i "$BUILD" src
