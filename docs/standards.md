# Opencraft 世界规范（简版）

Opencraft 是一个单进程体素游戏：C++20 + 手写 Vulkan 渲染器，用原生线程池生成和网格化
区块，玩法与数值对齐早期 Minecraft（Beta ~ 1.8），另外实现了下界与末地两个维度及传送门。

- 世界结构的数值常量只有一处来源：`src/specs.hpp`（区块列 16×128×16、海平面 62 等）。
- 默认命名空间是 `opencraft:`；方块、物品、实体、维度的规范 ID 分别在
  `src/blocks.hpp`、`src/items.hpp`、`src/entity.hpp`、`src/dimensions.hpp`。
- 存档是自定义二进制格式（`src/save.cpp`，魔数沿用 `VMSV`，版本 v2），
  每个存档一个目录，数据放在其中的 `level.dat`。

与 Minecraft 的对照资料见 `docs/minecraft_1.0_reference.md`。
