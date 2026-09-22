# Opencraft 项目状态（面向 AI 的上下文文档）

> 本文件是"压缩上下文"式的项目现状说明：换一个 AI/会话接手时，读这一份 + `AGENTS.md`
> 就能继续干活。**改动项目后请同步更新下面的「当前状态」一节。**
>
> 最后更新：末地/下界按原版机制重写完成（版本 `0.4.0-snapshot-2`）。

---

## 1. 项目是什么

Opencraft 是一个 C++20 写的单进程体素沙盒游戏，自带手写 Vulkan 渲染器（不依赖图形引擎），
玩法与数值对齐 Minecraft Java 版：

- 基础玩法对齐早期版本（Beta ~ 1.8）：草方块/矿石/橡树/海洋、创造模式飞行、快捷栏与背包。
- **下界与末地两个维度按 Java 版 26.2 的原版机制实现**（密度函数、结构、群系），
  反编译源码在 `D:\i\decompiled\26.2`（另一个是 `1.21.4`，26.2 更可读）。
- 主世界地形仍是早期的简易实现，**后续计划也翻新成原版机制**（尚未开始）。

构建产物 `build/opencraft.exe`，旁边需要 `assets/`、`shaders/*.spv`、`version.txt`。

## 2. 当前状态

### 已完成

| 模块 | 状态 |
|------|------|
| 渲染（Vulkan + 手写管线） | 可用；地形/水/天空/UI/菜单管线，图集 + mipmap |
| 区块系统 | 16×128×16 区块列，多线程生成 + 网格化，按维度分开存储 |
| 主世界生成 | 简易噪声（高度图 + 洞穴 + 矿石 + 树 + 水），非原版，待翻新 |
| **下界生成** | **已按 26.2 原版机制重写**：`nether/base_3d_noise` + slide 密度、基岩地板/天花板渐变、y=32 岩浆海、下界荒地群系表面规则 |
| **末地生成** | **已按 26.2 原版机制重写**：`end_islands` + `base_3d_noise` 密度、主岛、10 根黑曜石柱（含铁栏杆笼）、返回传送门、末地平台、外岛、紫颂植株 |
| 末地传送门框架 | 有朝向（facing）与有眼两种状态，共 8 个方块 id；13/16 高 + 眼块突出（多 element 模型） |
| 末地门激活 | 12 个框架**朝向必须指向环心**且都有眼；有自检 `portalselftest` |
| 维度传送 | 下界门（8:1 缩放）、末地门（进末地落在 y=49 的黑曜石平台） |
| 实体 | 末影龙、末影水晶、龙蛋（部分） |
| 存档 | 自定义二进制 v3，**按维度分段**保存/读取 |
| 紫颂花生长 | 方块随机刻（`mctick`）复刻 `ChorusFlowerBlock.randomTick` |
| 火把/铁栏杆/紫颂方块 | 已注册，有贴图 |

### 未完成 / 已知差距

- **主世界生成**没按原版，等后续翻新（用户明确说这是以后的事）。
- **末影龙 / 折跃门 / 龙蛋 / 末地城**暂不做（用户暂时不要）；龙相关代码是旧的，未与新生成对齐。
- 紫颂果（吃、传送）未做；紫颂花只做种植与生长。
- 下界群系只有 **下界荒地**；其他群系（灵魂沙峡谷、绯红/诡异森林、玄武岩三角洲）未做。
- 末地群系只做 **the_end**（中心）；外岛群系按"距离 > 64 区块"判定，没有做完整的多噪声群系选择。
- 下界的表面规则（灵魂沙/沙砾层）用"最高方块 + 同频噪声"近似，**没有**完整实现原版的
  `stone_depth` / `surface noise`；视觉效果接近但细节不完全一致。
- 外岛/紫颂的 feature 随机种子用固定盐而不是原版的全局 feature index（`setFeatureSeed`），
  结构长相与原版一致但**具体位置与原版不同种子不一致**。
- 火把/墙火把是普通全方块（没有原版的细杆模型，也没有朝向），铁栏杆也没有连接模型
  （只有 id 与贴图）；返回传送门柱上的四个火把朝向因此不完全正确。
- 存档不存方块状态/实体，只存 uint8 方块 id 数组。

### 最近一次验证

- `cmake --build build` 通过；
- `build/opencraft.exe --version`、`--menu-shot build/menu.png` 正常；
- `build/endprobe.exe 12345 7`：末地地形有山有谷（顶面高度 40~64），有黑曜石柱/传送门/紫颂统计；
- `build/portalselftest.exe`：8 项方环几何用例全部通过；
- 截图脚本：`--dim end` / `--dim nether` + `--pos/--yaw/--pitch` + `--screenshot out.png --no-ui`。

## 3. 目录与文件职责

### 构建与资源

| 路径 | 作用 |
|------|------|
| `CMakeLists.txt` | 主程序 + `extract_assets` / `endprobe` / `portalselftest` 三个小工具 + 打包目标 |
| `cmake/gen_version.cmake` | 构建时拼版本号（`version.txt` 的 `[current]` + 时间戳） |
| `version.txt` | 版本名唯一来源；只改 `[current]` 行，别写时间戳 |
| `shaders/*.vert/.frag` | GLSL，构建时 `glslc` 编译成 `build/shaders/*.spv` |
| `assets/block`、`assets/item`、`assets/gui` | 官方贴图（**不入库**，由 `extract_assets` 从玩家 jar 提取） |
| `tools/extract_assets.cpp` | 从 `minecraft.jar` 提取所需贴图到 `assets/`；新增方块贴图要加在这里 |
| `tools/pack_7z.ps1`、`7-Zip/` | 7-Zip SFX 自解压安装包（`releases/Opencraft-Installer.exe`） |
| `docs/standards.md` | 世界结构规范（常量来源、命名空间、存档格式） |
| `docs/minecraft_1.0_reference.md` | MC 1.0 对照参考 |
| `PROJECT_STATE.md` | **本文件** |

### 小工具（开发用，不随包分发）

| 工具 | 作用 |
|------|------|
| `tools/endprobe.cpp` → `build/endprobe.exe` | 打印末地高度剖面、方块统计、密度中间量（排查生成问题先跑它） |
| `tools/portalselftest.cpp` → `build/portalselftest.exe` | 末地门 5×5 方环朝向规则自检（8 个用例） |

### 核心源码 `src/`

**基础/规范**

| 文件 | 作用 |
|------|------|
| `specs.hpp` | 世界常量唯一来源：`CHUNK_SIZE=16`、`WORLD_HEIGHT=128`、`SEA_LEVEL=62`、坐标换算（floor 语义） |
| `dimensions.hpp` | 三维度表：Y 范围、坐标缩放、天光/天花板、雾与天空色 |
| `util.hpp` | 小数学库（`Vec3`/`Mat4`/`Frustum`）、`hash32`、`Rng` |
| `blocks.hpp` | 方块注册表（id/中文名/是否透明/碰撞/发光/衰减/六面图块）+ 图块枚举 + 末地框架朝向辅助 |
| `items.hpp` | 物品注册表 + 图块基址 |
| `entity.hpp` / `entities.hpp` | 实体基类与末影龙/末影水晶等具体实体 |

**世界与生成**

| 文件 | 作用 |
|------|------|
| `world.hpp/.cpp` | 世界：按维度的区块表、任务队列、工作线程、方块读写、实体管理、区块卸载 |
| `generator.hpp/.cpp` | **维度分发入口** `gen::generateForDim`；主世界生成（简易）；下界/末地转发到各自文件 |
| `nethergen.hpp/.cpp` | 下界生成（原版密度 + 表面规则 + 下界荒地） |
| `endgen.hpp/.cpp` | 末地生成（原版密度 + 黑曜石柱 + 返回传送门 + 末地平台 + 外岛 + 紫颂） |
| `mcnoise.hpp/.cpp` | **原版噪声复刻**：`McCnRandom`(LegacyRandomSource)、`McImprovedNoise`、`McSimplexNoise`、`McPerlinNoise`、`McBlendedNoise` |
| `noise.hpp/.cpp` | 早期简易 value noise（主世界还在用） |
| `mesher.hpp/.cpp` | 区块网格化：面剔除、AO、多 element 方块模型（末地门框架）、8 字节顶点 |
| `mctick.hpp/.cpp` | 方块随机刻（紫颂花生长） |
| `portal.hpp/.cpp` | 下界门框架识别与点燃、末地门方环识别与激活、门破碎 |
| `raycast.hpp/.cpp` | 方块射线检测 |
| `save.hpp/.cpp` | 存档（v3，按维度分段） |

**渲染与界面**

| 文件 | 作用 |
|------|------|
| `vk.hpp/.cpp`、`volk_impl.c` | Vulkan 设备/交换链/内存/管线基建 |
| `renderer.hpp/.cpp` | 渲染器：地形/水/天空/实体/UI/背包、区块缓冲上传、截图 |
| `atlas.hpp/.cpp` | 图集：读 `assets/block/*.png`，缺失/需要程序化的图块代码生成 |
| `shaders/terrain.vert` 等 | 着色器；`terrain.vert` 里用顶点第 4 字节还原 y 的小数部分 |
| `window.hpp/.cpp` | Win32 窗口、输入、鼠标捕获 |
| `menu.hpp/.cpp` | 主菜单/存档选择/新建世界/选项/暂停 |
| `camera.hpp/.cpp` | 相机（`yaw=0` 朝 +Z 即南，`forward=(sin yaw,*,cos yaw)`） |
| `player.hpp/.cpp` | 玩家物理、碰撞、飞行、游泳 |
| `png.hpp/.cpp` | PNG 读写（截图、贴图） |
| `version.hpp/.cpp` | 版本号读取与打印 |
| `main.cpp` | 命令行参数、主循环、菜单状态机、维度传送、方块交互 |

## 4. 关键机制与坑（改代码前必读）

### 4.1 方块只能是 uint8 id，没有状态位

原版一个方块可以有 facing/age/half 等状态，本工程 `Chunk::blocks` 是纯 `uint8_t`。
所以：

- **末地传送门框架**：朝向 × 有眼 = 8 种状态，展开成 8 个独立 id
  （`B_END_PORTAL_FRAME` 是"南"，`_W/_N/_E` 是其余三向，`B_END_PORTAL_FRAME_EYE*` 同理）。
  辅助函数在 `blocks.hpp`：`blockIsPortalFrame` / `blockFrameHasEye` / `blockFrameFacing` /
  `frameId(facing,eye)` / `frameFacingDir`。
  - **注意**：`B_END_PORTAL_FRAME` = 27，是"南"，**不是**原版的默认朝北。
  - 新增框态时必须同时改 `Block` 枚举、`BLOCK_DEFS` 和
    `blocks.hpp` 末尾的 `static_assert`（数组长度必须等于 `B_COUNT`）。
- 如果以后要加更多有状态的方块，正确做法是给方块加独立的状态字节/调色板，
  而不是继续把状态展开成 id（会把 id 空间撑爆，并让存档变大）。

### 4.2 生成必须"逐区块自足、与顺序无关"

`World::generateChunk` 在工作线程里只写自己这一块，**不会**写邻块。
所以：

- 地形（密度函数）天然自足，没问题。
- **结构/植被跨区块时要"把邻域候选都算一遍，只保留落在本区块的方块"**：
  外岛就是遍历 3×3 邻域区块各跑一次 `end_island`，`put()` 丢弃不属于本块的方块。
- 不要图省事在生成时 `world->setBlock`，那会依赖邻块是否已生成、且顺序不同结果不同。

### 4.3 末地密度函数的求值顺序不能"化简"

`noise_settings/end.json` 的 `slide` 形态是：

```text
s = y_clamped_gradient(56,312, 1,0) * (sloped_cheese + 23.4375)
t = lerp(y_clamped_gradient(4,32, 0,1), -0.234375, s)
final = squeeze(0.64 * (t - 23.4375))
```

**必须保持 `lerp -> 减 23.4375 -> 乘 0.64 -> squeeze` 的顺序。**
把 `-23.4375` 折进 `lerp` 的参数里（代数上看着等价）会让整片地形变成实心——
这个 bug 实际发生过，排查花了不少时间（症状：`endprobe` 里密度恒为 0.45833）。

其他相关事实：

- 末地的格是 **水平 8×8、垂直 4** 方块（`size_horizontal=2,size_vertical=1`）；
  但 `NoiseInterpolator` 对每个方块插值，所以本实现按 **4×4×4** 采样再三线性插值，结果等价。
- `end_islands` 的 simplex 种子固定是 `LegacyRandomSource(0)`（先 `consumeCount(17292)`），
  **与世界种子无关**，这是原版行为，不要"修"成用世界种子。
- `base_3d_noise` 用 `legacy_random_source: true` 的路径：`LegacyRandomSource(世界种子)`，
  再按 min/max/main 的顺序创建三段 `PerlinNoise`（八度 -15..0、-15..0、-7..0）。
  **三段的创建顺序不能变**，否则噪声完全不同。
- 末地主岛半径约 100 方块；中央 `-32..32` 区域几乎是整柱实心（原版也这样）。

### 4.4 随机刻与特征种子

- 紫颂花生长靠 `mctick::tickRandomBlocks`，主循环每帧调用（内部按 20TPS 累积）。
  它复刻 `ChorusFlowerBlock.randomTick`，用 `B_CHORUS_FLOWER`(活) / `B_CHORUS_FLOWER_DEAD`(枯萎)
  两种 id 代替原版的 `AGE 0..4 / 5`；因此分叉次数比原版略多（原版 age>=4 不再分叉）。
- 外岛/紫颂的 per-chunk 随机源用"世界种子 + 区块坐标 + 固定盐"合成，
  没有复刻原版的 `setFeatureSeed(decorationSeed, globalFeatureIndex, step)`。

### 4.5 方块模型（多 element）

`mesher.cpp` 的 `buildParts()` 把一个方块拆成若干长方体（1/16 格坐标）：

- 普通方块 = 一个 `0..16` 的整块。
- 末地门框架 = `0..13` 高的块；有眼时再加一个 `13..16` 高的眼块，眼块贴着朝向那一侧。
- `TerrainVertex` 是 8 字节：`x,y,z`(int8) + `fracY`(uint8, y 的 1/16 小数) + `u,v,tex,shade`。
  着色器 `terrain.vert` 里 `y += inPos.w / 16.0`（`w` 存 16 个 1/8 单位）。

### 4.6 相机 yaw 约定

`forward = (sin yaw, sin pitch, cos yaw)`：`yaw=0` 看 **+Z（南）**，`yaw=π/2` 看 +X（东）。
放末地门框架时朝向取玩家朝向的反方向（`portal::frameIdForPlacement`）。

## 5. 常用命令

```sh
# 构建（Ninja）
cmake -G Ninja -S . -B build
cmake --build build

# 冒烟
build/opencraft.exe --version
build/opencraft.exe --menu-shot build/menu.png

# 看末地/下界（截图模式；--frames 大一点等区块生成完）
build/opencraft.exe --dim end    --seed 12345 --pos 0,130,190 --yaw 3.14159 --pitch -0.42 \
                    --render-dist 10 --frames 400 --screenshot build/end.png --no-ui
build/opencraft.exe --dim nether --seed 12345 --pos 0,70,0      --yaw 0.7    --pitch -0.1  \
                    --render-dist 8  --frames 300 --screenshot build/nether.png --no-ui

# 生成自检
build/endprobe.exe 12345 7        # 末地高度剖面 + 方块统计
build/portalselftest.exe          # 末地门方环朝向规则

# 打包
cmake --build build --target package_7z
```

命令行参数（`src/main.cpp` 顶部）：`--seed --dim(overworld|nether|end) --pos x,y,z --yaw --pitch
--render-dist --threads --frames --no-ui --no-vsync --screenshot --menu-shot --menu-screen
--place x,y,z,id --break x,y,z --crystal x,y,z --drive --inventory --inv-page --spawn-portal
--gpu-index --time --version`。

## 6. 约定速查

- 注释、提交信息、用户可见文本一律**简短中文**，一行能说清就不写两行，不复述代码。
- 版本号只改 `version.txt` 的 `[current]`。
- `.ps1` 含中文必须存成 UTF-8 **带 BOM**；`.cpp/.hpp` 存成 UTF-8 **不带 BOM**
  （用 PowerShell `Set-Content` 会写成 ANSI 把中文弄坏，用 `[IO.File]::WriteAllText` +
  `UTF8Encoding($false)`；改完务必确认文件头不是 `EF BB BF`、中文不花）。
- 提交要少而清晰，一个提交一件事（方便回退），别攒成一个巨型提交。
- 新增方块贴图：加到 `tools/extract_assets.cpp` 的清单、`assets/block/`（本地）、
  `atlas.cpp` 的 `kTileFiles` + `kTileTint`。
