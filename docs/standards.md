# VoxMine 世界规范（对齐 Minecraft Java Edition）

> 目标：把 VoxMine 的世界结构、坐标、方块、实体、光照、维度、运行模型等
> 概念与 Minecraft（Java 版，下称 MC）的公开规范对齐，形成单一权威文档。
> 与 MC 不同的取值均为**当前实现的刻意简化**，每处都标注了 MC 规范值与演进方向。
>
> 本文档描述的常量全部集中在 `src/specs.hpp`（世界结构）、`src/entity.hpp`（实体）、
> `src/blocks.hpp`（方块注册表）。改动数值必须同时更新本文件。
>
> 参考资料为 Minecraft Wiki（minecraft.wiki / zh.minecraft.wiki）相关页面，
> 汇总见文末 [参考资料](#15-参考资料)。

## 目录

1. [概述与术语表](#1-概述与术语表)
2. [命名空间 ID（Resource location）](#2-命名空间-idresource-location)
3. [坐标系](#3-坐标系)
4. [区块（Chunk）与子区块（Section）](#4-区块chunk与子区块section)
5. [高度、海平面与世界边界](#5-高度海平面与世界边界)
6. [方块（Block）与方块注册表](#6-方块block与方块注册表)
7. [方块状态与方块实体（预留）](#7-方块状态与方块实体预留)
8. [实体（Entity）与玩家](#8-实体entity与玩家)
9. [光照（Light）（预留）](#9-光照light预留)
10. [维度（Dimension）（预留）](#10-维度dimension预留)
11. [游戏刻（Tick）与运行模型](#11-游戏刻tick与运行模型)
12. [存档格式](#12-存档格式)
13. [世界生成规范](#13-世界生成规范)
14. [与 Minecraft 的差异对照表](#14-与-minecraft-的差异对照表)
15. [参考资料](#15-参考资料)

## 1. 概述与术语表

VoxMine 是一个单进程体素游戏。本规范按 MC 的概念体系定义其世界模型，
使后续更新（新方块、实体、光照、联机）与模组制作有稳定的基础。

| 术语 | 英文 | 含义 |
|---|---|---|
| 方块 | Block | 世界的基本组成单元，1×1×1，占据整数坐标格 |
| 方块状态 | Block state | 一个方块位置的属性集合（预留，见 §7） |
| 方块实体 | Block entity | 携带额外数据的方块（预留，见 §7） |
| 实体 | Entity | 有命名空间 ID、按刻模拟、带碰撞箱的动态世界对象（§8） |
| 区块 | Chunk | 16×16 水平 × 全世界高的方块列（§4） |
| 子区块 | Section / Sub-chunk | 16×16×16 的立方存储段（§4） |
| 维度 | Dimension | 独立的世界空间（预留，§10） |
| 命名空间 ID | Resource location | `namespace:path` 形式的注册表键（§2） |
| 游戏刻 | Game tick | 逻辑模拟的最小时间单位，20 TPS（§11） |
| 光照等级 | Light level | 0–15 共 16 级（预留，§9） |
| 注册表 | Registry | 启动时静态注册的 ID→对象 映射（§2、§6） |
| 高度图 | Heightmap | 每列最高相关方块的 Y 值（§4） |

## 2. 命名空间 ID（Resource location）

MC 规范：所有注册表条目（方块、物品、实体类型、维度、生物群系、方块实体类型等）
都用 `namespace:path` 形式的字符串唯一寻址。

- 格式：`namespace:path`，如 `minecraft:stone`。
- 字符集：namespace 与 path 仅允许小写 `[a-z0-9._-]`；path 额外允许 `/`。
- 省略 namespace 时默认 `minecraft`（原版内容）。
- mod 用自带 namespace（如 `my_mod:xxx`）避免冲突。

VoxMine 规定：

- **默认命名空间为 `voxmine:`**。全部内置方块已在方块注册表
  （`src/blocks.hpp` 的 `BLOCK_DEFS[]`）中登记了规范 ID，
  如 `voxmine:stone`、`voxmine:grass_block`、`voxmine:oak_log`。
- 运行时方块 id 是 `uint8_t` 内部索引（0–18），仅在本进程内存与当前存档格式
  （§12）中使用；**未来存档 v3 / 联机协议以命名空间 ID 为准**。
- 实体类型的规范 ID 已定义：`voxmine:player`（`src/entity.hpp`）。
- 未来接入兼容层时，可建立 `minecraft:stone ⇄ voxmine:stone` 的映射表，
  使模组/资源包可按 MC 惯例寻址。

## 3. 坐标系

MC 规范：X 轴向东 (+X)，Y 轴向上 (+Y)，Z 轴向南 (+Z)。
方块坐标是整数；实体坐标是 double，且 Y 表示**脚部**高度。

VoxMine 采用与 MC 完全一致的轴向与单位约定：

| 坐标 | 含义 | 类型 | 取值范围（当前实现） |
|---|---|---|---|
| 方块坐标 (x,y,z) | 一个方块格 | 整数 | x,z ∈ ℤ；y ∈ [0,127] |
| 位置坐标 | 连续空间点（眼睛/实体） | float | 同上，y 超界被夹紧 |
| 区块坐标 (cx,cz) | 方块坐标换算 | 整数 | `floor(方块坐标/16)` |
| 局部坐标 (lx,lz) | 区块内偏移 | 整数 | `[0,16)` |

- 区块换算对**负坐标同样向下取整**：x=-1 属于区块 -1 的局部 x=15
  （等价于 MC 的 `x >> 4` / `x & 15` 语义）。
  统一使用 `src/specs.hpp` 的 `blockToChunkCoord()` / `blockToChunkLocal()`，
  禁止在调用处手写除法（旧代码的本地 `floordiv` 已删除）。
- 玩家相机位置 `Player::cam.pos` 是**眼睛**位置；实体位置 `Player::ent.pos`
  是**脚部中心**（MC 实体 Pos 语义），两者由 `Player::syncEntity()` 同步。
- 世界底面：y<0 为虚空，方块读写被拒绝，玩家脚下被夹紧在 y=0
  （MC 主世界底面为 y=-64，见 §14）。

## 4. 区块（Chunk）与子区块（Section）

MC 规范（Java 版）：

- 区块水平尺寸 **16×16**；1.18+ 区块列 **16×384×16**，Y ∈ [-64, 319]，
  由 **24 个** 16×16×16 的 section 垂直堆叠。
- 1.17 及更早为 16×256×16；最早为 16×128×16。
- 区块 k 覆盖世界坐标 `[k*16, k*16+16)`。
- 区块存储：32×32 个区块装进一个 region 文件（Anvil 格式，4 KiB 扇区），
  每个 section 用调色板（palette）+ 位宽索引压缩存储。
- 高度图类型：`WORLD_SURFACE`、`OCEAN_FLOOR`、`MOTION_BLOCKING`、
  `MOTION_BLOCKING_NO_LEAVES`（及生成期 `_WG` 变体）。

VoxMine 现状（`src/specs.hpp`）：

| 常量 | 值 | MC 对应 |
|---|---|---|
| `CHUNK_SIZE` | 16 | 区块水平边长 16（一致） |
| `WORLD_HEIGHT` | 128 | 1.18+ 为 384（简化） |
| `CHUNK_VOL` | 32768 | 单区块方块数 |
| `SECTION_SIZE` / `SECTION_VOL` | 16 / 4096 | section 边长/体积（一致） |
| `SECTION_COUNT` | 8 | 1.18+ 为 24 |

- 区块运行时状态：`0=空 → 1=已生成 → 2=已网格化`
  （`Chunk::state`，`src/world.hpp`）。
- 区块键 `chunkKey(cx,cz)` = 高 32 位 cx、低 32 位 cz 打包成 uint64。
- 区块内索引 `chunkIndex(lx,y,lz) = lx + (lz<<4) + (y<<8)`——y-major 布局，
  未来引入 section 存储时按 `SECTION_SIZE` 分段即可。
- **区块界限规则**：跨区块的面剔除/AO 需要邻区块数据。网格化
  （`World::meshChunk`）通过 18×18 的 `MeshView` 拷入四邻与四角的边界层数据；
  修改边界方块时必须把本区块与 4 邻都标记重网格化（`World::setBlock` 已实现）。
- 高度图：当前未存储独立高度图；生成器在生成期用 `localTop` 记录每列表面，
  渲染用 maxY 扫描裁剪。未来按 MC 语义补 `MOTION_BLOCKING` 等。
- 卸载：玩家所在区块切比 ∗ 曼哈顿距离超过 `renderDist+2` 的干净区块被回收。

## 5. 高度、海平面与世界边界

| 项 | MC 规范（主世界） | VoxMine 现状 |
|---|---|---|
| 可建造 Y 范围 | [-64, 319] | [0, 127]（`WORLD_MIN_Y`/`WORLD_MAX_Y`） |
| 海平面 sea level | Y=63 | Y=62（`SEA_LEVEL`，生成器以此为水位基准） |
| 虚空 | min_y 以下坠落死亡 | y<0 拒绝方块读写、玩家夹紧 |
| 世界边界 | 默认直径 29,999,984（每侧 ±14,999,992），硬上限 ±30,000,000 | 未强制；预留 `WORLD_BORDER_RADIUS`/`WORLD_BORDER_MAX` |

- 深度基准：MC 的方块 Y 是"方块格的整数坐标"；海平面 63 指水面标高，
  最高水面方块在 y=62。VoxMine 的 `SEA_LEVEL=62` 指水面标高，
  最高水方块 y=61（`generateColumn` 向下灌水至 `SEA_LEVEL-1`）。
- 出生点：MC 在世界原点附近搜索出生点；VoxMine 以区块 (0,0) 中心方块
  `CHUNK_SIZE/2` 为锚螺旋搜索，要求地面非水且上方 3 格无碰撞
  （`main.cpp` 出生搜索）。

## 6. 方块（Block）与方块注册表

MC 规范：每种方块 = 命名空间 ID + 属性集（不透明性、碰撞箱、发光等级、
贴图等）；所有方块在启动时静态注册进注册表。

VoxMine 的方块注册表在 `src/blocks.hpp`：

```cpp
struct BlockDef {
    const char* id;        // 命名空间 ID，如 "voxmine:stone"
    const char* name;      // 显示名（中文）
    bool        opaque;    // 完全遮挡（面剔除）
    bool        solid;     // 有碰撞箱
    uint8_t     lightEmission; // 发光等级 0..15（光照引擎预留，当前全 0）
    uint8_t     opacity;   // 光照衰减 0..15
    uint8_t     tileTop, tileSide, tileBottom; // 图集 tile（顶/侧/底）
};
inline constexpr BlockDef BLOCK_DEFS[B_COUNT] = { ... };
```

- 属性查询 `blockIsOpaque / blockIsSolid / blockTile / blockDef` 全部由
  注册表驱动；**新增方块只需在 `BLOCK_DEFS[]` 加一行**，不需要改引擎逻辑。
- 越界 id 容错：查询函数把 ≥B_COUNT 的 id 视为不透明/有碰撞（与旧行为一致），
  `blockDef()` 回落为空气定义。
- 当前 19 种方块的 opacity 语义对齐 MC：不透明块 15、树叶 1、水 1、
  空气/玻璃 0。
- 运行时 id（uint8 0–18）与存档格式绑定（§12），规范 ID 见 `BLOCK_DEFS[].id`。

当前方块清单（id / 规范 ID / 显示名）：
0 `voxmine:air` 空气 · 1 `voxmine:stone` 石头 · 2 `voxmine:grass_block` 草方块 ·
3 `voxmine:dirt` 泥土 · 4 `voxmine:bedrock` 基岩 · 5 `voxmine:cobblestone` 圆石 ·
6 `voxmine:oak_planks` 橡木木板 · 7 `voxmine:oak_log` 橡木原木 ·
8 `voxmine:oak_leaves` 橡木树叶 · 9 `voxmine:sand` 沙子 · 10 `voxmine:gravel` 沙砾 ·
11 `voxmine:coal_ore` 煤矿石 · 12 `voxmine:iron_ore` 铁矿石 ·
13 `voxmine:gold_ore` 金矿石 · 14 `voxmine:diamond_ore` 钻石矿石 ·
15 `voxmine:redstone_ore` 红石矿石 · 16 `voxmine:water` 水 ·
17 `voxmine:snow` 雪块 · 18 `voxmine:glass` 玻璃

## 7. 方块状态与方块实体（预留）

MC 规范：

- **方块状态（Block state）**：一个方块位置的具体属性配置
  （朝向 facing、点亮 lit、生长年龄 age 等）。
- **方块实体（Block entity）**：需要携带额外 NBT 数据的方块才维护
  方块实体（按需存储），例如箱子（物品栏）、熔炉（燃料/进度/产物）、
  告示牌（文字）。

VoxMine 预留设计（当前无任何方块状态/方块实体，**不新增内容**）：

- 方块状态：当前 `uint8_t` 只能表达"方块种类"。引入 blockstate 时，
  推荐把 `(id, state)` 编码为注册表中的"状态实例"（MC 的做法，
  palette 天然兼容），或扩展为 `uint16_t`。
- 方块实体：按 MC 语义**按需**挂接——`Chunk` 增加稀疏的
  `map<blockIndex, BlockEntityData>`，只对有数据的方块存储；
  类型用命名空间 ID（如 `voxmine:chest`）寻址，注册进方块实体类型表。
- 存档 v3 需要同时保存方块实体数据（见 §12）。

## 8. 实体（Entity）与玩家

MC 规范：实体是按刻模拟的动态对象，类型由命名空间 ID 决定，类型决定碰撞箱。
实体公共 NBT 字段（`Entity_format`）：
`id`、`Pos`（3×double，**脚部中心**）、`Motion`（3×double）、
`Rotation`（[yaw,pitch]，角度制）、`UUID`（4×int）、`Health`、
`Invulnerable`、`OnGround`、`Air`、`Fire`、`FallDistance`、`NoAI`、
`CustomName`、`Tags`、`PersistenceRequired`、`Dimension`（1.16+）。

玩家碰撞箱（MC）：站立 0.6×1.8；潜行 1.5；爬行/游泳 0.6×0.6；
眼高 1.62（潜行 1.27）。

VoxMine 的实体模型在 `src/entity.hpp`（当前**只有玩家一种实体**，未新增内容）：

```cpp
using EntityId = uint32_t;                    // 运行时实例 id，0=无效
struct EntityType { const char* id; float width, height, eyeHeight; };
inline constexpr EntityType ENTITY_PLAYER{"voxmine:player", 0.6f, 1.8f, 1.62f};
struct Entity { EntityId id; const EntityType* type; Vec3 pos, vel; float yaw, pitch; };
```

- `Entity` 字段与 MC NBT 一一对应：`pos`↔`Pos`、`vel`↔`Motion`、
  `yaw/pitch`↔`Rotation`（**MC 为角度制，VoxMine 内部为弧度制**，
  存档/联机序列化时须转换）、`type->id`↔`id`。
- `Player` 内嵌 `Entity ent`；碰撞箱尺寸字段（`height/halfWidth/eyeHeight`）
  统一取自 `ENTITY_PLAYER`，保证唯一来源。
- 坐标约定：`cam.pos` = 眼睛；`ent.pos` = 脚部中心；
  `Player::update()` 末尾调用 `syncEntity()` 保持同步。
- 实例持久化：MC 用 UUID；当前 `EntityId` 仅运行时有效，
  存档 v3 玩家记录应改存 UUID（§12）。
- 未来扩展：掉落物/生物 = 新 `EntityType` 注册 + `Entity` 派生数据，
  移动/物理复用 `Player::update` 的 AABB 碰撞例程（抽出即可）。

## 9. 光照（Light）（预留）

MC 规范：**方块光（block light）**与**天空光（sky light）**是两条独立通道，
每格各存 0–15 共 16 级：

- 光进入一个方块时衰减 `max(1, opacity)`；不透明方块 opacity=15（完全阻挡）。
  水/树叶 opacity=1，空气/玻璃 0。
- 光源等级：萤石/海晶灯/灯笼/岩浆 15，火把 14，灵魂火把 10，岩浆块 3。
- 天空光白天从天顶垂直向下不衰减（露天=15），渲染时乘日光系数
  （daylight factor）表现昼夜；内部值仍存 0–15。
- 光照影响玩法：多数敌对生物只在 block light=0 处生成；
  不死生物白天暴露在高 sky light 下燃烧。

VoxMine 现状：光照引擎**未实现**（渲染用烘焙面亮度 + AO）。
已就位的预留（`src/specs.hpp` + `src/blocks.hpp`）：

- 常量 `LIGHT_LEVELS=16`、`MAX_LIGHT=15`。
- 每个方块的 `lightEmission`（光源等级）与 `opacity`（衰减）已入注册表，
  当前全部光源为 0；引入光照引擎时按
  "BFS 传播、每步衰减 `max(1, opacity)`、天空光垂直特殊规则"实现，
  存储建议按 section（16³）分块，两张 4-bit 通道图。

## 10. 维度（Dimension）（预留）

MC 规范：维度 = 独立世界空间，由 dimension type 描述关键参数。
三大主维度：

| 字段 | overworld | the_nether | the_end |
|---|---|---|---|
| `min_y` / `height` | -64 / 384 | 0 / 256 | 0 / 256 |
| `coordinate_scale` | 1.0 | 8.0（下界 1 格 = 主世界 8 格） | 1.0 |
| `has_skylight` | true | false | false |
| `has_ceiling` | false | true | false |
| `ultrawarm` | false | true | false |
| `natural` | true | false | false |
| `ambient_light` | 0.0 | 0.1 | 0.0 |
| `logical_height` | 384 | 128 | 256 |

VoxMine 现状：**只有主世界一个维度**（隐式，等价 overworld 简化版），
代码中无维度概念。预留路线：

- 引入 `DimensionType`（命名空间 ID + 上表字段）与 `World*` 按维度分表；
  区块键扩展为 `(dim, cx, cz)`（高 32 位可再分出维度槽位）。
- 玩家/实体 NBT 增加 `Dimension` 字段（MC 1.16+ 语义）。
- 存档目录按维度分文件（MC：`region/`、`DIM-1/region/`、`DIM1/region/`）。

## 11. 游戏刻（Tick）与运行模型

MC 规范：**20 TPS，每刻 50ms**，是逻辑模拟的最小时间单位。
方块更新分 random tick（随机刻：作物生长等，`randomTickSpeed` 控制）
与 scheduled tick（计划刻：红石/流体等延后更新）；实体每刻更新一次。

VoxMine 现状：

- 主循环逐帧可变 dt（上限 0.05s），物理按 dt 积分；**尚无固定刻模拟**。
- 预留常量：`TICK_RATE=20`、`TICK_DT=0.05`（`src/specs.hpp`）。
- 联机路线（对齐 MC 客户端-服务端模型）：
  - **权威服务端**：服务端持有世界真值并按 20 TPS 模拟；
    客户端发送输入/动作，接收区块数据与实体状态快照。
  - 单人模式 = 集成服务端（MC 单人的做法），与专用服务端共用世界代码。
  - 世界线程池（生成/网格化 worker）已经独立于渲染线程，
    是未来把"世界模拟"剥离为服务端进程的基础。
- 客户端渲染侧约定：区块数据只读快照（`World::snapshotChunks`），
  网格化在 worker 完成，上传在主线程 `gpuSync` —— 与权威模拟解耦。

## 12. 存档格式

MC 规范：region 文件（32×32 区块，Anvil/NBT，section palette 压缩）+
`level.dat`（全局数据与玩家 NBT，玩家 `Pos` 为脚部坐标）。

VoxMine 现状（自定义二进制，`src/save.cpp`）：每个存档一个目录，
`level.dat` 单文件，格式 `VMSV` v2：

| 偏移 | 字段 | 类型 | 说明 |
|---|---|---|---|
| 0 | magic `VMSV` | u32 | 0x564D5356 |
| 4 | version | u32 | 当前 2 |
| 8 | seed | u32 | 世界种子 |
| 12 | spawnX/Y/Z | 3×f32 | **眼睛位置**（见下方偏差说明） |
| 24 | yaw/pitch | 2×f32 | 弧度制 |
| 32 | flying | u32 | 0/1 |
| 36 | numChunks | u32 | 后续区块数 |
| 40 | 每区块：cx(i32) cz(i32) blocks[32768] | — | 1 字节/方块，y-major |

- v2 在 v1 基础上追加了 yaw/pitch/flying（12 字节），读取器按 version 兼容。
- **已知偏差**：spawn 存的是眼睛坐标（MC level.dat 存实体脚部 `Pos`）；
  v3 应改为脚部坐标 + UUID，并迁移到命名空间 ID/方块状态/方块实体。
- 未保存的数据：网格（重载后强制重建）、光照（无）、实体（无）。
- 选项 `options.txt`（exe 旁）：`vsync:on|off`、`renderDist:N` ——
  **注意**：它会持久化渲染距离，做截图对比测试时务必用
  `--render-dist` 显式覆盖，否则两侧配置不一致会污染对比结果。

## 13. 世界生成规范

- **确定性**：同一 seed 必然生成同一世界。所有噪声/随机都由
  `(seed, cx, cz)` 派生（`src/generator.cpp`，含洞穴/矿物/树木/水体）。
- 地形：分层噪声（continents/hills/detail）叠加，高度夹紧到 [3, 127]；
  表层材质由温度/湿度噪声决定（沙漠/雪原/草甸/海滩/砾石滩）。
- 基岩：y≤4；洞穴：y∈[3,90) 3D 噪声；矿物按深度分布；树木避开区块边界
  （距边界 ≥2 方块），避免跨区块结构。
- 水体：海面/湖泊灌水至 `SEA_LEVEL-1`；陆地洞穴约 20% 概率局部含水。
- MC 对应概念：biome（生物群系）≈ `SurfaceInfo`（冷/热/干/湿）；
  结构跨区块生成（MC 的 feature 放置）当前**不支持**，树被约束在区块内。
- 出生搜索与 spawn 规范见 §5。

## 14. 与 Minecraft 的差异对照表

| 项 | Minecraft Java | VoxMine 现状 | 演进方向 |
|---|---|---|---|
| 区块列高度 | 384（Y -64..319，24 sections） | 128（Y 0..127，8 段概念） | section 化存储后可扩高度 |
| 海平面 | Y=63 | Y=62 | 生成器参数，可对齐 |
| 方块 ID | `minecraft:*` + blockstate | `voxmine:*` 调色板，uint8 | 存档 v3 用命名空间 ID |
| 方块状态 | 完整 blockstate 系统 | 无（纯种类） | 见 §7 |
| 方块实体 | 箱子/熔炉等按需 NBT | 无 | 见 §7 |
| 实体 | 完整实体系统 + UUID | 仅玩家（已实体化建模） | 新 EntityType 即可扩展 |
| 光照引擎 | 双通道 0–15 BFS 传播 | 无（烘焙亮度） | 常量/opacity 已预留（§9） |
| 维度 | 三维度 + 自定义维度类型 | 仅主世界 | 见 §10 |
| 存档 | Anvil region + NBT + palette | VMSV v2 单文件 | palette/分 region（§12） |
| 联机 | 权威服务端 + 客户端 | 单进程 | 集成/专用服务端（§11） |
| 模拟步长 | 固定 20 TPS | 可变 dt（≤0.05s） | 固定刻循环（§11） |
| 高度图 | 4 类（MOTION_BLOCKING 等） | 生成期 localTop | 运行时高度图 |
| 玩家速度 | 走 4.317 m/s | 4.5 m/s | 手感参数，可对齐 |
| 重力 | ≈32 m/s² | 28 m/s² | 手感参数，可对齐 |
| 触及距离 | 生存 4.5 / 创造 5 | 6（`raycastWorld` 调用） | 可对齐 |
| 世界边界 | 默认 ±14,999,992 强制 | 不强制 | 常量已预留（§5） |

> 手感参数（速度/重力/触及）刻意保留现状以不改变既有玩法；
> 对齐它们属于玩法调参，不属于本规范的结构改造。

## 15. 参考资料

- 区块与子区块：https://minecraft.wiki/w/Chunk 、https://minecraft.wiki/w/Chunk_format 、https://wiki.vg/Chunk_Format
- 坐标/海拔/海平面：https://minecraft.wiki/w/Altitude 、https://minecraft.wiki/w/Coordinates 、https://minecraft.wiki/w/Sea_level
- 世界边界：https://minecraft.wiki/w/World_border
- 高度图：https://minecraft.wiki/w/Heightmap
- 维度：https://minecraft.wiki/w/Dimension 、https://minecraft.wiki/w/Dimension_type 、https://minecraft.wiki/w/The_Nether
- 区域文件/Anvil：https://minecraft.wiki/w/Region_file_format 、https://minecraft.wiki/w/Anvil_file_format
- 光照/不透明度：https://minecraft.wiki/w/Light 、https://minecraft.wiki/w/Opacity 、https://minecraft.wiki/w/Mob_spawning
- 实体/玩家：https://minecraft.wiki/w/Entity 、https://minecraft.wiki/w/Entity_format 、https://minecraft.wiki/w/Player.dat_format 、https://minecraft.wiki/w/Sneaking
- 命名空间 ID：https://minecraft.wiki/w/Resource_location 、https://zh.minecraft.wiki/w/资源定位符
- 方块/方块实体：https://minecraft.wiki/w/Block 、https://minecraft.wiki/w/Block_entity
- 刻：https://minecraft.wiki/w/Tick
- 注册表：https://minecraft.wiki/w/Registry
- 中文站：https://zh.minecraft.wiki/w/区块 、https://zh.minecraft.wiki/w/光照 、https://zh.minecraft.wiki/w/刻

> 注：minecraft.wiki 对非浏览器客户端有 Cloudflare 拦截（HTTP 403），
> 上文数值经 web 检索对照 wiki 页面核实，均为 Java 版长期稳定的规范值。







