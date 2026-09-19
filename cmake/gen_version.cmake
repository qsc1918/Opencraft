# 由 CMake 在构建时执行：读 version.txt 的 [current] 行 + 当前时间戳 -> version_gen.hpp
# 用法：cmake -DROOT=<源码根> -DOUT=<输出头文件> -P gen_version.cmake

file(READ "${ROOT}/version.txt" _txt)
string(REPLACE "\r\n" "\n" _txt "${_txt}")
string(REPLACE "\n" ";" _lines "${_txt}")

# 第一个 [current] 行就是当前版本名，允许手动修改
# 注：这里用字面量比较，避免 CMake 正则里 [ ] 的转义坑
set(_name "")
foreach(_line IN LISTS _lines)
  string(STRIP "${_line}" _line)
  string(LENGTH "${_line}" _len)
  if(_len GREATER 9)
    string(SUBSTRING "${_line}" 0 9 _head)
    if(_head STREQUAL "[current]")
      string(SUBSTRING "${_line}" 9 -1 _name)
      string(STRIP "${_name}" _name)
      break()
    endif()
  endif()
endforeach()
if(_name STREQUAL "")
  set(_name "0.0.0-unknown")
endif()

# 编译时间戳（本地时间，便于比较新旧）
string(TIMESTAMP _stamp "%Y%m%d-%H%M%S")

get_filename_component(_outdir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_outdir}")
file(WRITE "${OUT}"
"// 本文件由 cmake/gen_version.cmake 自动生成，请勿手动修改
#pragma once
#define OPENCRAFT_VERSION_NAME \"${_name}\"
#define OPENCRAFT_BUILD_STAMP \"${_stamp}\"
")
message(STATUS "Opencraft 版本号: ${_name}-${_stamp}")
