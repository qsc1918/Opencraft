# Opencraft 世界规范（简版）

Opencraft 是一个单进程体素游戏：C++20 + 手写 Vulkan 渲染器，用原生线程池生成和网格化
区块，玩法与数值对齐早期 Minecraft（Beta ~ 1.8）。下界与末地已按 Minecraft Java 版
26.2 的原版机制实现（密度函数、结构、群系），主世界地形仍是简易实现、后续再翻新。

- 世界结构的数值常量只有一处来源：`src/specs.hpp`（区块列 16×128×16、海平面 62 等）。
- 默认命名空间是 `opencraft:`；方块、物品、实体、维度的规范 ID 分别在
  `src/blocks.hpp`、`src/items.hpp`、`src/entity.hpp`、`src/dimensions.hpp`。
- 方块运行期是纯 `uint8_t` id、没有状态位：末地传送门框架的 facing/eye 展开成 8 个 id，
  见 `blocks.hpp` 的 `frameId()` / `blockFrameFacing()`。
- 存档是自定义二进制格式（`src/save.cpp`，魔数沿用 `VMSV`，**当前版本 v3**）：
  v3 起每个维度一段（`dimId + count`，之后是 `cx, cz, blocks[32768]`），
  文件头带玩家所在维度；v1/v2 只有一段、按主世界读。
  每个存档一个目录，数据放在其中的 `level.dat`。
- 生成代码的约定：「逐区块自足、与顺序无关」——只写本区块的方块，跨区块的结构要把
  邻域候选算一遍再裁剪（见 `src/endgen.cpp` 的外岛）。

维度生成实现分布：入口 `src/generator.cpp`，下界 `src/nethergen.cpp`，末地
`src/endgen.cpp`，原版噪声复刻 `src/mcnoise.cpp`。

与 Minecraft 的对照资料见 `docs/minecraft_1.0_reference.md`，
项目现状与文件职责见 `PROJECT_STATE.md`。
