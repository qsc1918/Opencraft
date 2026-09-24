# Opencraft

Opencraft 是一个用 C++20 写的类 Minecraft 体素沙盒游戏，自带手写 Vulkan 渲染器和多线程
区块生成/网格化。基础内容参考早期 Minecraft：草方块与矿石、橡树、海洋。下界与末地两个
维度按 Minecraft Java 版 26.2 的**原版机制**实现（密度函数、黑曜石柱、返回传送门、外岛、
紫颂植株等）；主世界地形仍是简易实现，后续计划翻新。

项目现状、文件职责与开发约定见 `PROJECT_STATE.md` 与 `AGENTS.md`。

## 编译

依赖：Vulkan SDK、CMake ≥ 3.20、Ninja、MinGW GCC 12+ 或 MSVC。

```sh
cmake -G Ninja -S . -B build
cmake --build build
```

产物是 `build/opencraft.exe`；构建时会把 `assets/`、`shaders/*.spv`、`version.txt`
复制到它旁边。

> `assets/` 里的贴图从 Minecraft 官方 jar 提取，按 Mojang EULA 不随仓库分发。
> 请用 `tools/extract_assets` 从你自己的 Minecraft 安装中提取。

## 打包自解压安装包

用官方 7-Zip SFX 模块打包，需要仓库根目录有 `7-Zip\`（含 `7z.exe` 和 `7z.sfx`，
本地产物、不入库）。默认每次 `cmake --build build` 之后自动生成
`releases/Opencraft-Installer.exe`；没有 `7-Zip\` 时只警告并跳过，不影响构建。
不想要自动打包就关掉：

```sh
cmake -S . -B build -DOPENCRAFT_AUTO_PACKAGE=OFF
```

按需/手动打包：

```sh
cmake --build build --target package_7z    # releases/Opencraft-Installer.exe
powershell -File tools\pack_7z.ps1         # 也可以直接跑脚本，参数见脚本头部
```

## 自动发布（GitHub Actions）

推一个 `ci` 开头的 tag 就会在 GitHub 上自动构建并发布：

```sh
git tag ci0.4.0-snapshot-3
git push origin ci0.4.0-snapshot-3
```

workflow 在 `.github/workflows/release.yml`：在 `windows-latest` 上用 MSVC + Ninja +
Vulkan SDK 构建，跑 `package_7z` 打包，然后把 `releases/Opencraft-Installer.exe`
作为 Release 附件上传（安装包是自解压程序，里面已经含 exe、shaders、version.txt 和
`extract_assets.exe`，所以发布只需要这一个文件）。也可以在 Actions 页面手动触发，
手动触发只构建验证、不发布。

`assets/` 不入库，CI 上不存在，所以代码里避开它：CMakeLists 在 `assets/` 缺失时跳过
复制，构建和打包都照常成功。安装包本来也不含贴图，因此**玩家拿到安装包后必须先运行
包里的 `extract_assets.exe` 提取贴图，否则 `opencraft.exe` 会因为读不到
`assets/gui/button.png` 而直接退出**（`menu.cpp` 加载失败即启动失败）。这是遵守
Mojang EULA 的预期流程，不是 bug。

## 运行

```sh
build/opencraft.exe
```

## 版本号

`version.txt` 的 `[current]` 行是版本名（可手动改），编译时自动追加编译时间戳，
完整版本号形如 `0.4.0-snapshot-1-20260214-153012`，显示在主菜单右下角；
用 `opencraft.exe --version` 也可以直接打印。

## 操作

| 按键 | 说明 |
|------|------|
| W / A / S / D | 移动 |
| 鼠标 | 视角 |
| 空格 | 跳跃（飞行时上升）；双击切换飞行 |
| Shift | 飞行时下降 |
| E | 打开/关闭物品栏 |
| 左键 / 右键 | 挖掘 / 放置方块 |
| 1..9 / 滚轮 | 切换快捷栏 |
| T | 时间快进 |
| Esc | 暂停菜单；菜单中返回上一级 |

## 目录

- `src/` 源码：世界与区块、地形生成（`nethergen.cpp` / `endgen.cpp` / 原版噪声 `mcnoise.cpp`）、
  网格化、Vulkan 渲染、菜单、存档
- `shaders/` GLSL 着色器，构建时由 glslc 编译
- `tools/` 资源提取工具、自解压安装包脚本、开发自检工具（`endprobe`、`portalselftest`）
- `docs/` 项目说明；`PROJECT_STATE.md` 项目现状与文件职责

## 开发约定

- 注释一律用**简短的中文**，一两句话说清即可，不要长篇大论。
- 不要写英文或其他语言的注释。
- Git 提交要少而清晰，提交信息也用简短中文。
- 其它约定见 `AGENTS.md`。
