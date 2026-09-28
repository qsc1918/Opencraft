@echo off
chcp 65001 >nul
cmake -G Ninja -S . -B build
cmake --build build
pause