# Opencraft 开发约定（AI 入口）

Opencraft 是 C++20 写的类 Minecraft 体素沙盒游戏，自带手写 Vulkan 渲染器和多线程
区块生成/网格化。下界与末地已按 Minecraft Java 版 26.2 的原版机制实现；主世界地形
还是早期的简易实现，后续计划翻新。

**接手项目先读 `PROJECT_STATE.md`**：那里有完整的文件职责表、当前状态、未完成项和
踩过的坑。本文件只放约定和索引入口。

## 约定

- 注释一律用简体中文，尽量短：能一行就一行，只说明“为什么 / 约束 / 坑”，不复述代码。
- 不允许英文或其他语言的注释；新增的用户可见文本也尽量用中文。
- `.ps1` 脚本含中文时必须存成 UTF-8 带 BOM，否则 Windows PowerShell 5.1 会乱码。
  `.cpp/.hpp` 必须存成 UTF-8 **不带 BOM**；别用 `Set-Content`（会写成 ANSI 弄坏中文），
  用 `[IO.File]::WriteAllText($p, $t, (New-Object Text.UTF8Encoding($false)))`。
- 版本号只改 `version.txt` 的 `[current]` 行；编译时间戳由 `cmake/gen_version.cmake`
  在构建时自动追加，不要手写时间戳。
- Git 提交要少而清晰：一个提交一件事，方便回退；提交信息用简短中文，别攒巨型提交。
- 新增方块要同时改 `src/blocks.hpp` 的 `Block` 枚举、`BLOCK_DEFS`（顺序即 id），
  `B_COUNT` 和文件末尾的 `static_assert`；贴图还要进 `tools/extract_assets.cpp`
  和 `src/atlas.cpp` 的 `kTileFiles`/`kTileTint`。
- 生成代码必须"逐区块自足、与顺序无关"：只写本区块的方块，跨区块的结构要把邻域
  候选算一遍再裁剪（见 `src/endgen.cpp` 的外岛）。

## 构建 / 验证 / 打包

```sh
cmake -G Ninja -S . -B build
cmake --build build
build/opencraft.exe --version
build/opencraft.exe --menu-shot build/menu.png
build/endprobe.exe 12345 7      # 末地生成自检：高度剖面 + 方块统计
build/portalselftest.exe        # 末地门朝向规则 + 方环几何自检
build/portalflowtest.exe 12345  # 传送门流程自检：末地门插眼激活 + 下界门造门/回程落点
```

打包：默认构建后自动出 `releases/Opencraft-Installer.exe`（7-Zip SFX，开关
`OPENCRAFT_AUTO_PACKAGE`，缺 `7-Zip\` 时只警告）；也可用 `--target package_7z`。

截图调试：`build/opencraft.exe --dim end|nether --seed N --pos x,y,z --yaw R --pitch R
--render-dist N --frames 300 --screenshot out.png --no-ui`（`--frames` 要给够，等区块生成完）。

调试用参数（见 `PROJECT_STATE.md` §5）：`--tp-dim overworld|nether|end` 进世界后直接切维度
（游戏里 F7 正向、F6 反向循环切主世界/下界/末地）；`--place x,y,z,id` 放方块、
`--use-eye x,y,z` 对末地门框架用末影之眼、`--light-portal x,y,z` 点燃下界门。

## 目录入口

- `src/specs.hpp` 世界常量唯一来源；`src/dimensions.hpp` 维度表；`src/blocks.hpp` 方块表。
- `src/generator.cpp` 按维度分发的入口；下界在 `src/nethergen.cpp`，末地在 `src/endgen.cpp`。
- `src/mcnoise.cpp` 原版噪声/随机数复刻（LegacyRandomSource、ImprovedNoise、SimplexNoise、
  PerlinNoise、BlendedNoise）；改生成前先确认这里的参数与顺序。
- `src/world.cpp` 区块存储与工作线程；`src/mesher.cpp` 网格化与多 element 方块模型；
  `src/renderer.cpp` 渲染；`src/portal.cpp` 传送门。
- `docs/standards.md` 世界规范；`docs/minecraft_1.0_reference.md` MC 对照资料。
- 反编译源码在 `D:\i\decompiled\26.2`（首选）与 `D:\i\decompiled\1.21.4`。

## 当前状态

见 `PROJECT_STATE.md` 的「当前状态」一节（完成项、未完成项、最近验证、已知差距）。
改完项目请同步更新那一节。
