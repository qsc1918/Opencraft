cmake_minimum_required(VERSION 3.20)
project(opencraft CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Vulkan SDK：优先用环境变量，否则用默认安装路径
set(VK $ENV{VULKAN_SDK})
if(NOT VK)
  set(VK "C:/VulkanSDK/1.4.357.0")
endif()

add_executable(opencraft
  src/main.cpp
  src/window.cpp
  src/vk.cpp
  src/png.cpp
  src/atlas.cpp
  src/noise.cpp
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
target_include_directories(opencraft PRIVATE src ${CMAKE_CURRENT_BINARY_DIR} ${VK}/Include ${VK}/Include/volk)
target_compile_definitions(opencraft PRIVATE VK_NO_PROTOTYPES)
target_compile_options(opencraft PRIVATE -O3 -march=native -fno-rtti)
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
set(SPV ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(GLSLC ${VK}/Bin/glslc.exe)
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
add_custom_command(TARGET opencraft POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_CURRENT_SOURCE_DIR}/version.txt ${CMAKE_CURRENT_BINARY_DIR}/version.txt
  # 编译结束后更新标记文件，使下次构建必然重编 version.cpp
  COMMAND ${CMAKE_COMMAND} -E touch ${VERSION_STAMP}
)

# ---- 附带的小工具（打包安装包要用）----
# 资源提取工具：控制台程序，从玩家的 minecraft.jar 提取贴图
add_executable(extract_assets tools/extract_assets.cpp)
target_link_libraries(extract_assets PRIVATE comdlg32)

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
