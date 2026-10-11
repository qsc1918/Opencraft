# mingw-w64 交叉编译工具链：在 Linux 上编出 Windows 版
#
#   sudo apt install mingw-w64 g++-mingw-w64-x86-64
#   cmake -S . -B build-mingw -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake
#   cmake --build build-mingw
#
# 产物是 build-mingw/opencraft.exe（自带 MinGW 运行库，拷到 Windows 就能跑）。
# 注意：菜单（GDI）在 Windows 上才有，所以这个目标会带上菜单代码，正常。
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

# 静态链接运行时，产物不依赖 MinGW 的 DLL
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++ -static")
set(CMAKE_C_FLAGS_INIT   "-DWIN32")
set(CMAKE_CXX_FLAGS_INIT "-DWIN32")

# 用宿主机的 CMake 跑 syscall，避免"用 MinGW 的 ld 当成宿主链接器"
set(CMAKE_CROSSCOMPILING_EMULATOR /usr/bin/cmake)
