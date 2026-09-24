cmake_minimum_required(VERSION 3.20)
project(opencraft CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Vulkan SDK：优先用环境变量；没有就在默认安装目录里挑版本号最大的那个
# （不能写死版本号：CI runner 和别的机器装的版本不一样）
set(VK "$ENV{VULKAN_SDK}")
if(NOT VK OR NOT EXISTS "${VK}/Include/vulkan/vulkan.h")
  file(GLOB _vk_candidates "C:/VulkanSDK/*")
  set(_vk_best "")
  foreach(_d ${_vk_candidates})
    if(EXISTS "${_d}/Include/vulkan/vulkan.h")
      set(_vk_best "${_d}")
    endif()
  endforeach()
  set(VK "${_vk_best}")
endif()
if(NOT VK)
  message(FATAL_ERROR "找不到 Vulkan SDK：请设置 VULKAN_SDK 环境变量，或把 SDK 装到 C:/VulkanSDK/<版本>")
endif()
message(STATUS "Vulkan SDK: ${VK}")

# 平台宏：两套编译器都要，所以放全局
#  NOMINMAX           windows.h 默认定义 min/max 宏，会破坏 std::max/std::min
#                     （GCC/MinGW 头文件顺序碰巧躲过，MSVC 下是 error C2589）
#  _USE_MATH_DEFINES  MSVC 默认不定义 M_PI
add_compile_definitions(NOMINMAX _USE_MATH_DEFINES)

# 编译选项：MSVC 与 GCC/Clang 两套写法。原来只写了 GCC 那套，
# 导致 CI 上（windows-latest 用 cl.exe）配置阶段就失败。
# 放在所有 target 之前：add_compile_options 只对它之后定义的 target 生效。
if(MSVC)
  # /MT 静态链接运行库，否则没装 VC++ 可再发行组件的机器跑不起来
  # /GR- 关 RTTI，对应 GCC 的 -fno-rtti；/permissive- 收紧标准一致性
  # /utf-8 必须加：源码是 UTF-8 无 BOM 且含中文注释，MSVC 不指定时会按系统
  #   代码页（中文机器是 GBK）解析，部分中文字节序列会吃掉换行，把源码结构
  #   解析坏（GCC 默认就是 UTF-8，所以本地不暴露这个问题）。
  add_compile_options(/MT /GR- /permissive- /EHsc /utf-8)
  # 不用 /arch:AVX2：CI 在服务器 CPU 上编译，产物要能在普通玩家机器上跑
else()
  # 只给 C++：volk_impl.c 是 C 文件，收到 -fno-rtti 会刷警告
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
endif()

add_executable(opencraft
  src/main.cpp
  src/window.cpp
  src/vk.cpp
  src/png.cpp
  src/atlas.cpp
  src/noise.cpp
  src/mcnoise.cpp
  src/endgen.cpp
  src/nethergen.cpp
  src/mctick.cpp
  src/generator.cpp
  src/world.cpp
  src/mesher.cpp
  src/raycast.cpp
  src/portal.cpp
  src/camera.cpp
  src/player.cpp
  src/renderer.cpp
  src/save.cpp
  src/menu.cpp
  src/version.cpp
  src/volk_impl.c
)

# 生成头文件目录（版本号）也要能被包含
# volk 走仓库内的 third_party/volk，不用 SDK 的：SDK 里是 Include/Volk（大写），
# 原来写的 Include/volk 只在大小写不敏感的文件系统上侥幸命中，CI 上直接找不到
# volk.h（fatal error C1083）。自带一份也让本地与 CI 行为一致。
target_include_directories(opencraft PRIVATE src third_party/volk ${CMAKE_CURRENT_BINARY_DIR} ${VK}/Include)
target_compile_definitions(opencraft PRIVATE VK_NO_PROTOTYPES)
# 优化等级交给 CMake 的 Release 配置（MSVC 是 /O2，GCC 是 -O3）；
# 原来写死的 -O3 和 -march=native 在 MSVC 下是错误参数，-march=native 还会让
# CI 编出的二进制依赖 runner 的指令集、在玩家机器上非法指令崩溃。
target_link_libraries(opencraft PRIVATE user32 gdi32 ole32 oleaut32)

# 版本号：编译时读 version.txt 的 [current]，再拼上当前时间戳
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(VERSION_STAMP ${GEN_DIR}/version_stamp)
# 先保证标记文件存在，否则首次构建时依赖解析会失败
file(WRITE ${VERSION_STAMP} "0\n")

add_custom_target(version_gen ALL
  COMMAND ${CMAKE_COMMAND}
          -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
          -DOUT=${GEN_DIR}/version_gen.hpp
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_version.cmake
  COMMENT "生成版本号（version.txt + 编译时间戳）"
  VERBATIM
)
add_dependencies(opencraft version_gen)

# 让 version.cpp 每次构建都重新编译，这样时间戳才是本次编译的
set_source_files_properties(src/version.cpp PROPERTIES OBJECT_DEPENDS ${VERSION_STAMP})

# shaders
# glslc 不能写死成 ${VK}/Bin/glslc.exe：CI 上 SDK 布局一变就找不到工具，
# 而且报错信息会很难懂。先用 SDK 的 Bin，再退回 PATH。
set(SPV ${CMAKE_CURRENT_BINARY_DIR}/shaders)
find_program(GLSLC NAMES glslc glslc.exe HINTS "${VK}/Bin" DOC "glslc 着色器编译器")
if(NOT GLSLC)
  message(FATAL_ERROR "找不到 glslc：请安装 Vulkan SDK（需要 ${VK}/Bin/glslc.exe）")
endif()
message(STATUS "glslc: ${GLSLC}")
set(SHADER_FILES
  terrain.vert terrain.frag
  water.vert water.frag
  ui.vert ui.frag
  sky.vert sky.frag
  menu.vert menu.frag
)
set(SPV_TARGETS "")
foreach(s ${SHADER_FILES})
  add_custom_command(
    OUTPUT ${SPV}/${s}.spv
    COMMAND ${GLSLC} -O -o ${SPV}/${s}.spv ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${s}
    DEPENDS shaders/${s}
    COMMENT "glslc ${s}"
  )
  list(APPEND SPV_TARGETS ${SPV}/${s}.spv)
endforeach()
add_custom_target(shaders ALL DEPENDS ${SPV_TARGETS})
add_dependencies(opencraft shaders)

# 资源与版本文件复制到 exe 旁边（程序按相对路径读取）
set(COPY_EXTRA)
# assets/ 按 Mojang EULA 不入库，CI 上不存在；源目录缺失时 copy_directory 会直接
# 报 "file COPY cannot find" 让整个构建失败，所以这里必须先判断再复制。
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets)
  list(APPEND COPY_EXTRA COMMAND ${CMAKE_COMMAND} -E copy_directory
       ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets)
else()
  message(STATUS "assets/ 不存在（未提取贴图）：跳过资源复制，打出的包不含贴图")
endif()

add_custom_command(TARGET opencraft POST_BUILD
  ${COPY_EXTRA}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_CURRENT_SOURCE_DIR}/version.txt ${CMAKE_CURRENT_BINARY_DIR}/version.txt
  # 编译结束后更新标记文件，使下次构建必然重编 version.cpp
  COMMAND ${CMAKE_COMMAND} -E touch ${VERSION_STAMP}
)

# ---- 附带的小工具（打包安装包要用）----
# 资源提取工具：控制台程序，从玩家的 minecraft.jar 提取贴图
add_executable(extract_assets tools/extract_assets.cpp)
target_link_libraries(extract_assets PRIVATE comdlg32)

# 末地生成调试工具：打印高度剖面与方块统计（开发用，不入包）
add_executable(endprobe tools/endprobe.cpp src/endgen.cpp src/mcnoise.cpp)
target_include_directories(endprobe PRIVATE src)

# 末地门方环几何自检（只用 blocks.hpp/portal.hpp 的头文件逻辑，不链接 World）
add_executable(portalselftest tools/portalselftest.cpp)
target_include_directories(portalselftest PRIVATE src)

# 传送门流程自检（末地门激活 + 下界门目的地生成/回程，链接 World）
add_executable(portalflowtest
  tools/portalflowtest.cpp
  src/portal.cpp
  src/world.cpp
  src/generator.cpp
  src/endgen.cpp
  src/nethergen.cpp
  src/mcnoise.cpp
  src/noise.cpp
  src/mesher.cpp
)
target_include_directories(portalflowtest PRIVATE src)
target_link_libraries(portalflowtest PRIVATE user32)

# ---- 打包自解压安装包（7-Zip SFX）----
# 需要仓库根目录有 7-Zip\（本地产物，不入库）；默认每次构建后自动打包
option(OPENCRAFT_AUTO_PACKAGE "每次构建后自动打包安装包" ON)

add_custom_target(package_7z
  COMMAND powershell -NoProfile -ExecutionPolicy Bypass
          -File ${CMAKE_CURRENT_SOURCE_DIR}/tools/pack_7z.ps1
          -BuildDir ${CMAKE_CURRENT_BINARY_DIR}
          -OutDir ${CMAKE_CURRENT_SOURCE_DIR}/releases
  COMMENT "打包自解压安装包（releases/Opencraft-Installer.exe）"
  VERBATIM
)
add_dependencies(package_7z opencraft extract_assets)

# 自动打包：没有 7-Zip 时只警告不报错（-Optional），不打断正常构建
add_custom_target(package_7z_auto
  COMMAND powershell -NoProfile -ExecutionPolicy Bypass
          -File ${CMAKE_CURRENT_SOURCE_DIR}/tools/pack_7z.ps1
          -BuildDir ${CMAKE_CURRENT_BINARY_DIR}
          -OutDir ${CMAKE_CURRENT_SOURCE_DIR}/releases
          -Optional
  COMMENT "自动打包安装包（没有 7-Zip 就跳过）"
  VERBATIM
)
add_dependencies(package_7z_auto opencraft extract_assets)

if(OPENCRAFT_AUTO_PACKAGE)
  add_custom_target(package_auto ALL)
  add_dependencies(package_auto package_7z_auto)
endif()
